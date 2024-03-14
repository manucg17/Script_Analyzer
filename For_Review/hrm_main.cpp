#include <string.h>

#include "nedbg.h"
#include "aos_utils.h"
#include "sig_utils.h" // signal handing
#include "platform.h"
#include "keeperClient.h"
#include "HRM_ActionHandler.h"
#include "HRM_Server.h"


NeEngine           gReactor; // Note: This name require by keeperSrv
KeeperSrv         *gHrmSrv = NULL;
HRM_ActionHandler *gHrmActionHandler = NULL;

// reqiured by neEngine
int allow_severity;
int deny_severity;


int quitHRMCB()
{
    NE_DEBUG(DL_NOTIFY, ("%s - Started : Shutting down by keeper!!!",__FUNCTION__));

    if (gHrmSrv) {
        reinterpret_cast<HRM_Server*>(gHrmSrv)->stop();
    }

    NE_DEBUG(DL_NOTIFY, ("%s - Finished",__FUNCTION__));
    return 1;
}


int main (int argc,  char*  argv[])
{
    // set common keeper parameters from command line 
    argc = AosUtils::Instance()->init(argc, argv);

    NE_DEBUG_START ("HRM");
    NE_DEBUG (DL_NOTIFY,("%s - HRM started.",__FUNCTION__));

    NE_DEBUG (DL_TRACE,("%s - Call set_signal_handlers()",__FUNCTION__));
    set_signal_handlers();

    Infra_Rc rc;
	if ((rc = platform_init()) != IRC_OK) {
		NE_DEBUG(DL_ERROR,("Failed to initialize platform API (rc = [%d]).", rc));
		return rc;
	}

    // create keeper client and connect to parent swKeeper
    NE_DEBUG (DL_TRACE,("%s - Create gKeeperClient(%p,KEEPER_SEC_SERVER)",__FUNCTION__,&gReactor));
    keeperClient gKeeperClient(&gReactor, KEEPER_SEC_SERVER);

    //register quit callback that will be called when keeper will stop HRM
    NE_DEBUG (DL_TRACE,("%s - Call gKeeperClient.set_quit(...)",__FUNCTION__));
    gKeeperClient.set_quit((kpQuitCB)&quitHRMCB);

    // create HRM server
    NE_DEBUG (DL_TRACE,("%s - new HRM_Server(%p,%p)",__FUNCTION__,&gReactor,&gKeeperClient));
    gHrmSrv = new HRM_Server(&gReactor, &gKeeperClient);
    if (gHrmSrv == NULL) {
        NE_DEBUG(DL_ERROR,("%s - Can't allocate HRM_Server",__FUNCTION__));
        NE_DEBUG (DL_NOTIFY,("%s - HRM exited (-1).",__FUNCTION__));
        return -1;
    }

    // HRM initialization as keeper server
    gHrmActionHandler = new HRM_ActionHandler();
    if (gHrmActionHandler == NULL) {
        NE_DEBUG(DL_ERROR,("%s - Can't allocate ActionHandler",__FUNCTION__));
        NE_DEBUG (DL_TRACE,("%s - Call delete gHrmSrv",__FUNCTION__));
        delete gHrmSrv;
        NE_DEBUG (DL_NOTIFY,("%s - HRM exited (-2).",__FUNCTION__));
        return -2;
    }
    char sw_conf_dir[KEEPER_CMD_LEN];
    sprintf(sw_conf_dir, "%s/", SWG_CONF);
    NE_DEBUG (DL_TRACE,("%s - Call gHrmSrv->init(%p,\"%s\")",__FUNCTION__,gHrmActionHandler,sw_conf_dir));
    gHrmSrv->init(gHrmActionHandler, sw_conf_dir);

    // run server main loop
    NE_DEBUG (DL_TRACE,("%s - Call gHrmSrv->run()",__FUNCTION__));
    gHrmSrv->run();
    
    // ON TERMINATE
    if (gHrmSrv) {
        NE_DEBUG (DL_TRACE,("%s - Call delete gHrmSrv",__FUNCTION__));
        delete gHrmSrv;
    }
    if (gHrmActionHandler) {
        NE_DEBUG (DL_TRACE,("%s - Call delete gActionHandler",__FUNCTION__));
        delete gHrmActionHandler;
    }

    NE_DEBUG (DL_NOTIFY,("%s - HRM exited.",__FUNCTION__));
    return 0;

}



