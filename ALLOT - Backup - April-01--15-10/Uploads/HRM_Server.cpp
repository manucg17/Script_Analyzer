
#include <sys/stat.h>

#include "nedbg.h"
#include "sys_util.h"  
#include "messages.h"
#include "halApi.h"
#include "hal_card.h"
#include "productId.h"
#include "aos_utils.h"
#include "SysConfStorage.h"
#include "HRM_Server.h"
#include "HRM_State.h"
#include "HRM_StateElection.h"
#include "HRM_StateDisabledMaster.h"
#include "HRM_StateDisabledSlave.h"
#include "HRM_PeerClient.h"
#include "HRM_SystemMgrClient.h"
#include "HRM_StateStandby.h"
#include "acprof.h"
#include "platform.h"
#include "HRM_DataSyncDefs.h"



extern const char * __redundancy_status_str[];

#define HRM_ENABLE_CONNECTIVITY_TMOUT    30
#define HRM_DISABLE_CONNECTIVITY_TMOUT   15
#define HRM_VERIFY_DATA_SYNC_RECEIVED_COUNT_LIMIT (3*60)

// hrm.conf file paramters
#define HRM_CONF_FILE_NAME              SWG_CONF "/hrm.conf"
#define HRM_CONF_HRM_SECTION_NAME       "HRM"
#define HRM_CONF_HRM_MODE_FIELD_NAME    "HRM_ENABLE"
#define HRM_CONF_HRM_MODE_FIELD_DEFAULT HRM_MODE_DISABLE_E
#define HRM_UNDEFINED_PEER_IP_ADDR      "0.0.0.0"

//-------------------------------------------------------------------------------------------
// handleSWStatusEvent 
//
// evnet handler for EVENT_GRP_REDUNDANCY/EVENT_ID_DEV_SW_STATUS
//-------------------------------------------------------------------------------------------
keeperStatus handleSWStatusEvent (unsigned int grp, unsigned int id, void *data, size_t size, void *context)
{
    NE_DEBUG(DL_TRACE,
             ("%s - Started (grp=%d,id=%d,data=%p,size=%" FMTSIZEOF ",context=%p)",
             __FUNCTION__,grp,id,data,size,context));

    if (grp != EVENT_GRP_REDUNDANCY || !(id & EVENT_ID_DEV_SW_STATUS) || !context) {
        NE_DEBUG(DL_WARNING,
                 ("%s - Unknown paramters (grp=%d,id=%d,context=%p) - ignore",
                 __FUNCTION__,grp,id,context)); 
        return KEEPER_FAILURE;
    }

    HRM_Server* hrmSrv = reinterpret_cast<HRM_Server*>(context);
    ConfigAll* eventData = reinterpret_cast<ConfigAll*>(data);

    if(eventData->data.redund.board_sw_status_capability) {
        hrmSrv->handleSWStatusUpdate(eventData->data.redund.device_status);
    }
    else {
        NE_DEBUG(DL_DEBUG,
            ("%s - SKIP SW status update from non board level (grp=%d,id=%d,context=%p)", __FUNCTION__,grp,id,context));
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;
}

//-------------------------------------------------------------------------------------------
// handleReinitDoneEvent 
//
// event handler for  EVENT_GRP_GEN_NO_DATA/EVENT_ID_GEN_SW_REINIT_DONE
//-------------------------------------------------------------------------------------------
keeperStatus handleReinitDoneEvent (unsigned int grp, unsigned int id, void *data, size_t size, void *context)
{
    NE_DEBUG(DL_TRACE,
             ("%s - Started (grp=%d,id=%d,data=%p,size=%" FMTSIZEOF ",context=%p)",
             __FUNCTION__,grp,id,data,size,context));

    if (grp != EVENT_GRP_GEN_NO_DATA || !(id & EVENT_ID_GEN_SW_REINIT_DONE) || !context) {
        NE_DEBUG(DL_ERROR,
                 ("%s - Unknown paramters (grp=%d,id=%d,context=%p) - ignore",
                 __FUNCTION__,grp,id,context)); 
        return KEEPER_FAILURE;
    }

    HRM_Server* hrmSrv = reinterpret_cast<HRM_Server*>(context);
    hrmSrv->handleReinitCompleted();

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;
}

/*!
 * @brief Event handler to process the DBAgent finished provisioning notification.
 * In some cases DBAgent fails to load the catalog and doesn't send an init to proceed with the boot sequence. HRM waits for
 * a specified amount of time for AOS initialization. If it times out, then card is rebooted. In case DBAgent fails to load
 * the catalogs, then we should not reboot the card, since doing so would result in a reboot loop. When provisioning is
 * completed a flag is set here. Only if this flag is set HRM reboots the card upon software ready timeout.
 *
 * @param [in] event group
 * @param [in] event id
 * @param [in] event data
 * @param [in] event data size
 * @param [in] context
 *
 * @return return KEEPER_FAILURE if incorrect data is received, otherwise return KEEPER_SUCCESS
 */
keeperStatus handleDBAgentFinishedProvisioning(unsigned int grp, unsigned int id, void *data, size_t size, void *context)
{
    NE_DEBUG(DL_TRACE, ("%s - Started (grp=%d, id=%d, data=%p, size=%" FMTSIZEOF ", context=%p)", __FUNCTION__, grp, id, data, size, context));

    if (grp != EVENT_GRP_HRM || !(id & EVENT_ID_DBAGENT_PROV_DONE)) {
        NE_DEBUG(DL_WARNING, ("%s - Unknown parameters (grp=%d, id=%d, context=%p) - ignore", __FUNCTION__, grp, id, context));
        return KEEPER_FAILURE;
    }

    NE_DEBUG(DL_INFO, ("%s - Received DBAgent finished provisioning event", __FUNCTION__));
    HRM_Server* hrmSrv = reinterpret_cast<HRM_Server*>(context);

    // Restarts the SoftwareReadyTimeout timer. During startup HRM waits for init from DBAgent((triggered once catalogs are loaded)
    // before rebooting the device.If the init from DBAgent gets delayed , then HRM will timeout before the full initialization
    // of other modules happen and device will reboot.Added a fix to restart the HRM timer once provisioning is completed
    // so that other modules get enough time to start initialization
    hrmSrv->rescheduleSwReadyTimeoutTimer();
    hrmSrv->setDBAgentFinishedProvisioning(true);

    NE_DEBUG(DL_TRACE, ("%s - Finished", __FUNCTION__));
	return KEEPER_SUCCESS;
}

/*!
 * @brief event handler for the event from LTM indicating that all internal ports were brought up successfully.
 *
 * @param [in] grp
 * @param [in] id
 * @param [in] data
 * @param [in] size
 * @param [in] context
 *
 * @return return KEEPER_FAILURE if incorrect data is received, otherwise return KEEPER_SUCCESS
 */
keeperStatus handlePortBootDoneEvent (unsigned int grp, unsigned int id, void *data, size_t size, void *context)
{
    NE_DEBUG(DL_TRACE,("%s - Started (grp=%d,id=%d,size=%" FMTSIZEOF ",context=%p)", __func__, grp, id, size, context));

    if (grp != EVENT_GRP_LINK || !(id & EVENT_ID_LINK_PORT_BOOT_UPDATE) || !context) {
        NE_DEBUG(DL_WARNING,("%s - Unknown paramters (grp=%d,id=%d,context=%p) - ignore", __func__, grp, id, context));
        return KEEPER_FAILURE;
    }

    HRM_Server* hrmSrv = reinterpret_cast<HRM_Server*>(context);
    // Restarts the SoftwareReadyTimeout timer. During startup HRM waits for internal ports to be up
    // If this gets delayed, then HRM will timeout before other modules are completely initialized
    // and device will reboot. Added a fix to restart the HRM timer once internal ports are brought up
    // so that other modules get enough time to start initialization
    hrmSrv->rescheduleSwReadyTimeoutTimer();
	hrmSrv->setLocalPortBootDone(true);

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;
}
//-------------------------------------------------------------------------------------------
// HRM_Server notifyStartInstall
//                                   
// static callback to notify start installation via keeperMgr
//-------------------------------------------------------------------------------------------
keeperStatus    HRM_Server::notifyStartInstall (void *pData, void* context)
{
    NE_DEBUG(DL_TRACE,("%s - Started (pData=%p,context=%p)",__FUNCTION__,pData,context));

    if(!pData || !context)
        return KEEPER_FAILURE;

    HRM_Server* hrmSrv = reinterpret_cast<HRM_Server*>(context);
    hrmSrv->notifyStartInstall();

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;
}


//////////////////////// class HRM_Server /////////////////////////////////////

HRM_Server::HRM_Server(NeEngine *engine, keeperClient* pKeeperCli) 
                   : KeeperSrv(engine, (RecoveryIfc*)0)
                   , m_hrmMode(HRM_MODE_UNDEFINED_E)
                   , m_hrmMasterSlotNum(HRM_MASTER_SLOT_7)
                   , m_forceStatusCfg(HOST_ACTST_NA)
                   , m_hrmCurrentState(NULL)
                   , m_hrmPeerTOTimer(-1)
                   , m_softwareReadyTimeoutTimer(-1)
                   , m_cfgSoftwareReadyActiveTimeout(0)
                   , m_cfgSoftwareReadyStandbyTimeout(0)
                   , m_hrmPeerCli(NULL)
                   , m_pKeeperCli(pKeeperCli)
                   , m_cardSwStatus(CHECK_STATE_NO_CHANGE)
                   , m_localPortBootDone(false)
                   , m_hrmStopped(false)
                   , m_DBAgentFinishedProvisioning(false)
                   , m_starttime(time(NULL))
{
    NE_DEBUG(DL_TRACE,("%s - Started (engine=%p,pKeeperCli=%p)",__FUNCTION__,engine,pKeeperCli));

    memset(&m_hrmPublicData, 0, sizeof(HRMData));
    memset(&m_hrmPeerData, 0, sizeof(HRMData));
    memset(&m_productDetails, 0, sizeof(m_productDetails));

    m_hrmPeerData.hrmHAStatus = HOST_ACTST_NA;

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server DTOR
// -------------------------------------------------------------
HRM_Server::~HRM_Server()
{
    NE_DEBUG(DL_TRACE,("%s - Started ",__FUNCTION__));

    if (m_hrmCurrentState) {
        delete m_hrmCurrentState;
        m_hrmCurrentState=NULL;
    }

    if (m_hrmPeerCli){
        delete m_hrmPeerCli;
        m_hrmPeerCli = NULL;
    }

    if (m_systemMgrCli) {
        delete m_systemMgrCli;
        m_systemMgrCli = NULL;
    }

    hal_systemClose();

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server stop
//                                   
// handle stop by keeper
// -------------------------------------------------------------
void HRM_Server::stop()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    if ((k_reactor) && (!m_hrmStopped)) 
        k_reactor->addTimerMillisec(5, this, (NeCallback) &HRM_Server::stopCb, 0, 0);

    m_hrmStopped = true;

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server stopReactorCb
//                                   
// stop NeEngine loop
// -------------------------------------------------------------
int HRM_Server::stopReactorCb   (void *cbData)
{
    NE_DEBUG(DL_TRACE,("%s - Started (cbData=%p)",__FUNCTION__,cbData));

    if (k_reactor)
        k_reactor->stop();

    NE_DEBUG(DL_NOTIFY, ("%s - Stop process Finished",__FUNCTION__));

    NE_DEBUG(DL_TRACE,("%s - Finished (TIMER_RUN_ONCE) ",__FUNCTION__));
    return TIMER_RUN_ONCE;
}

//-------------------------------------------------------------
// HRM_Server stopCb
//                                   
// stop Server functionality
// -------------------------------------------------------------
int HRM_Server::stopCb   (void *cbData)
{
    NE_DEBUG(DL_TRACE,("%s - Started (cbData=%p)",__FUNCTION__,cbData));

    NE_DEBUG(DL_NOTIFY, ("%s - Stop process started",__FUNCTION__));

    if (m_hrmCurrentState) {
        NE_DEBUG(DL_DEBUG,("%s - Call m_hrmCurrentState[%s]->handleHRMExit()",__FUNCTION__,m_hrmCurrentState->hrmStateName()));
        m_hrmCurrentState->handleHRMExit();
    }

    if (k_reactor)
        k_reactor->addTimerMillisec(5, this, (NeCallback) &HRM_Server::stopReactorCb, 0, 0);

    NE_DEBUG(DL_TRACE,("%s - Finished (TIMER_RUN_ONCE) ",__FUNCTION__));
    return TIMER_RUN_ONCE;
}

//-------------------------------------------------------------
// HRM_Server init
//                                   
// Get initial configuration 
// -------------------------------------------------------------
keeperStatus HRM_Server::_init_specific()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    // read product params from system configuration file
    string val;
    SysConfStorage srcSysConfStorage;
    if (srcSysConfStorage.init(SYSTEM_CFG_FILE)) {
        if(srcSysConfStorage.getCfgParamValue(SYSTEM_CFG_PRODID, val))
            m_productDetails.hwmodel = atoi(val.c_str());
        if(srcSysConfStorage.getCfgParamValue(SYSTEM_CFG_NSLOTS, val))
            m_productDetails.nslots = atoi(val.c_str());
    } else{
        NE_DEBUG(DL_WARNING,("%s - Failed to read number of slots from file %s.",__FUNCTION__,SYSTEM_CFG_FILE));
    }

    k_capabilities = 0;

    if ((SGVE200 == gPlatformInfo.productId) || (SGT2 == gPlatformInfo.productId)) // SGT2 or SGVE200
        m_hrmMasterSlotNum = HRM_MASTER_SLOT_1;

    // check if HRM is supported
    if (m_productDetails.nslots > 1){
        // load HRM enabled/disabled configuration
        int hrmMode;
        struct in_addr peerIpAddr;
        int actTimeOut = 0;
        int standbyTimeOut = 0;
        NE_DEBUG(DL_DEBUG,
                 ("%s - m_configHrmStorage. file name %s\n",
                  __FUNCTION__,HRM_CONF_FILE_NAME));
        if (m_configHrmStorage.init(HRM_CONF_FILE_NAME) != true) {
            NE_DEBUG(DL_ERROR,
                     ("%s - m_configHrmStorage.init(\"%s\") failed\n",
                     __FUNCTION__,HRM_CONF_FILE_NAME));
            hrmMode = HRM_MODE_DISABLE_E;
            inet_aton(HRM_UNDEFINED_PEER_IP_ADDR, &peerIpAddr);
        } else {
            string valMode;
            if (m_configHrmStorage.getCfgParamValue(CFG_HRM_MODE, valMode) != true) {
                NE_DEBUG(DL_ERROR,
                         ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) failed\n",
                          __FUNCTION__,CFG_HRM_MODE));
                hrmMode = HRM_MODE_DISABLE_E;
                inet_aton(HRM_UNDEFINED_PEER_IP_ADDR, &peerIpAddr);
            } else {
                NE_DEBUG(DL_DEBUG,
                         ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) val:%s\n",
                          __FUNCTION__,CFG_HRM_MODE,valMode.c_str()));
                hrmMode = atoi(valMode.c_str());
                if ((hrmMode != HRM_MODE_ENABLE_E) &&
                    (hrmMode != HRM_MODE_DISABLE_E) ) {
                   NE_DEBUG(DL_ERROR,
                            ("%s - Unknown %s value %d [%s] read from config file , set to disable\n",
                             __FUNCTION__,
                             CFG_HRM_MODE,
                             hrmMode,
                             valMode.c_str()));
                   hrmMode = HRM_MODE_DISABLE_E;
                   inet_aton(HRM_UNDEFINED_PEER_IP_ADDR, &peerIpAddr);
                }
            }
            // Getting configured active and standby swReadyTimeOut From Init file (/opt/allot/conf/hrm.conf)
            string valActTimeOut;
            if (m_configHrmStorage.getCfgParamValue(CFG_HRM_SW_READY_ACTIVE_TIMEOUT, valActTimeOut) != true) {
                NE_DEBUG(DL_ERROR,
                         ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) failed\n",
                          __FUNCTION__,CFG_HRM_SW_READY_ACTIVE_TIMEOUT));
            }
            else {
               NE_DEBUG(DL_DEBUG,
                                  ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) val:%s\n",
                                         __FUNCTION__,CFG_HRM_SW_READY_ACTIVE_TIMEOUT,valActTimeOut.c_str()));
               actTimeOut = atoi(valActTimeOut.c_str());
            }

            string valStandbyTimeOut;
            if (m_configHrmStorage.getCfgParamValue(CFG_HRM_SW_READY_STANDBY_TIMEOUT, valStandbyTimeOut) != true) {
                NE_DEBUG(DL_ERROR,
                         ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) failed\n",
                          __FUNCTION__,CFG_HRM_SW_READY_STANDBY_TIMEOUT));
            }
            else {
               NE_DEBUG(DL_DEBUG,
                                  ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) val:%s\n",
                                         __FUNCTION__,CFG_HRM_SW_READY_STANDBY_TIMEOUT,valStandbyTimeOut.c_str()));
               standbyTimeOut = atoi(valStandbyTimeOut.c_str());
            }
        }
		
        if (SGVE200 == gPlatformInfo.productId) {
            //Peer Ip is given by the vNFM in SGVE200 systems
            string valPeerIp;
            if (m_configHrmStorage.getCfgParamValue(CFG_HRM_PEER_IP, valPeerIp) != true) {
                NE_DEBUG(DL_ERROR,
                         ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) failed\n",
                          __FUNCTION__,CFG_HRM_PEER_IP));
                inet_aton(HRM_UNDEFINED_PEER_IP_ADDR, &peerIpAddr);
            } else {
                NE_DEBUG(DL_DEBUG,
                         ("%s - m_configHrmStorage.getCfgParamValue(\"%s\",val) val:%s\n",
                          __FUNCTION__,CFG_HRM_PEER_IP,valPeerIp.c_str()));
                if (!inet_aton(valPeerIp.c_str(), &peerIpAddr)) {
                   NE_DEBUG(DL_ERROR,
                            ("%s - Unknown %s value [%s] read from config file , set to zero\n",
                             __FUNCTION__,
                             CFG_HRM_PEER_IP,
                             valPeerIp.c_str()));
                   hrmMode = HRM_MODE_DISABLE_E;
                   inet_aton(HRM_UNDEFINED_PEER_IP_ADDR, &peerIpAddr);
                }
            }
        }

        m_hrmMode=(HrmMode)hrmMode;
        m_peerMgmtIp = peerIpAddr;
        m_cfgSoftwareReadyActiveTimeout=actTimeOut*60; // Timeout in seconds
        m_cfgSoftwareReadyStandbyTimeout=standbyTimeOut*60; // Timeout in seconds

        m_hrmPublicData.hrmMode = m_hrmMode;
        m_hrmPeerData.hrmMode = HRM_MODE_UNDEFINED_E;

        NE_DEBUG(DL_NOTIFY,("%s - HRM mode configuration: %s [%u].", __FUNCTION__,hrmMode2Str(m_hrmMode),m_hrmMode));

        // get my slot number
        // hal kpc library init 
        int32_t rc = hal_systemInit();
        if(rc == IRC_OK){
            int32_t slot = 1;
            rc = hal_slotIdGet(&slot);
            if (rc != IRC_OK){
                NE_DEBUG(DL_ERROR,("%s - Could not get slot id using HAL, rc = %d",__FUNCTION__,rc));
            }
            if (slot < 1)
                slot = 1;

            m_hrmPublicData.hrmSlotNum = slot;
            if (slot == m_hrmMasterSlotNum)
                m_hrmPeerData.hrmSlotNum = slot + 1; // I'm master slot 1/7, my peer slot 2/8
            else
                m_hrmPeerData.hrmSlotNum = slot - 1; // I'm slave slot 2/8, my peer slot 1/7

            NE_DEBUG(DL_TRACE,
                     ("%s - my slot %d, peer slot %d, master slot %d", 
                     __FUNCTION__,
                     m_hrmPublicData.hrmSlotNum,
                     m_hrmPeerData.hrmSlotNum,
                     m_hrmMasterSlotNum));
        } else {
            hal_systemClose();
            NE_DEBUG(DL_ERROR,("%s - Failed on hal init, hal_systemInit() [%d]",__FUNCTION__,rc));
        }
    }

    // register on SW status event
    if (m_pKeeperCli) { 
        m_pKeeperCli->register_event_ext(EVENT_GRP_REDUNDANCY, EVENT_ID_DEV_SW_STATUS, (kpEventCBExt)&handleSWStatusEvent, this);
        m_pKeeperCli->register_event_ext(EVENT_GRP_GEN_NO_DATA, EVENT_ID_GEN_SW_REINIT_DONE, (kpEventCBExt)&handleReinitDoneEvent, this);
        m_pKeeperCli->register_event_ext(EVENT_GRP_HRM, EVENT_ID_DBAGENT_PROV_DONE, (kpEventCBExt)&handleDBAgentFinishedProvisioning, this);
        m_pKeeperCli->register_event_ext(EVENT_GRP_LINK, EVENT_ID_LINK_PORT_BOOT_UPDATE, (kpEventCBExt)&handlePortBootDoneEvent, this);
        unsigned int cbID = 0;
        m_pKeeperCli->register_callback_ext("notifyStartInstall",
                                            "\t notify HRM about start installation",
                                            &notifyStartInstall,
                                            &cbID,
                                            this);
    }

    // read HRM LAT
    _readLAT();

    //initialize HRM connection to peer
    _connectToPeer();

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;

}

//-------------------------------------------------------------
// HRM_Server _initHRMState
//                                   
// initialize HRM state
// -------------------------------------------------------------
void    HRM_Server::_initHRMState()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    switch(m_hrmMode){
    case HRM_MODE_ENABLE_E:
        NE_DEBUG(DL_TRACE,("%s - HRM_MODE_ENABLE_E",__FUNCTION__));

        if(m_forceStatusCfg == HOST_ACTST_NA) {

            // run election
            m_hrmCurrentState = new HRM_StateElection(this);

        } else{ // force status from ini file

            if (m_forceStatusCfg == HOST_ACTST_ACTIVE){
                // force active SBH
                if( m_pKeeperCli) {
		    NE_DEBUG(DL_INFO,("%s - send event GEN_HOST_GOING_ACTIVE", __FUNCTION__));
                    m_pKeeperCli->send_event(EVENT_GRP_GEN_NO_DATA, EVENT_ID_GEN_HOST_GOING_ACTIVE, NULL, 0);
                }
                m_hrmCurrentState = new HRM_StateDisabledMaster(this);
            } else 
            if(m_forceStatusCfg  == HOST_ACTST_STANDBY){
                m_hrmCurrentState = new HRM_StateDisabledSlave(this, m_forceStatusCfg);
            }
            // send global init to keeper
            if(m_pKeeperCli)
                m_pKeeperCli->init();
        }
        break;

    case HRM_MODE_DISABLE_E:
        NE_DEBUG(DL_TRACE,("%s - HRM_MODE_DISABLE_E",__FUNCTION__));

        if (m_hrmPublicData.hrmSlotNum == m_hrmMasterSlotNum){ // HRM disable on master slot 1/7
            // go active SBH
            if (m_pKeeperCli){
		NE_DEBUG(DL_INFO,("%s - send event GEN_HOST_GOING_ACTIVE", __FUNCTION__));
                m_pKeeperCli->send_event(EVENT_GRP_GEN_NO_DATA, EVENT_ID_GEN_HOST_GOING_ACTIVE, NULL, 0);
            }
            m_hrmCurrentState = new HRM_StateDisabledMaster(this);
        }
        else{ // HRM disable on slave 2/8
            m_hrmCurrentState = new HRM_StateDisabledSlave(this, HOST_ACTST_NA);
        }

        // send global init to keeper
        if(m_pKeeperCli)
            m_pKeeperCli->init();
        break;

    default:
        NE_DEBUG(DL_WARNING,
                 ("%s - Unknown m_hrmMode %d - ignore it",__FUNCTION__,m_hrmMode));
        return; // single blades 
    }

    NE_DEBUG(DL_INFO,("%s - HRM is initialized with state [%s].",__FUNCTION__,m_hrmCurrentState->hrmStateName()));

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server _connectToPeer
//
// initialize HRM connection to peer
// -------------------------------------------------------------
void    HRM_Server::_connectToPeer()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    switch(m_hrmMode){
    case HRM_MODE_ENABLE_E:
        startHRMPeerConnectivity(HRM_ENABLE_CONNECTIVITY_TMOUT);
        break;

    case HRM_MODE_DISABLE_E:
        startHRMPeerConnectivity(HRM_DISABLE_CONNECTIVITY_TMOUT);
        break;

    default:
        NE_DEBUG(DL_WARNING,
                 ("%s - Unknown m_hrmMode %d - ignore it",__FUNCTION__,m_hrmMode));
        return; // single blades
    }

    // connect to systemManager
    m_systemMgrCli = new HRM_SystemMgrClient(k_reactor, this);
    if (m_systemMgrCli == NULL)
    {
    	NE_DEBUG(DL_ERROR,("HRM_Server::_connectToPeer m_systemMgrCli NULL pointer "))
    	return ;
    }


    m_systemMgrCli->setFlags(6                 ,  /* retryCnt      : 0                          */
                             true              ,  /* reconnect     : false                      */
                             KEEPER_IO_TIMEOUT ,  /* syncReqTO     : KEEPER_IO_TIMEOUT (3*1000) */
                             120               ,  /* syncKATimeout : 0                          */
                             true              ); /* immediateReg  : false                      */
    if(m_systemMgrCli)
        m_systemMgrCli->connect2Server();

    // Bug fix 43602: DUMMY event registration only for starting KA by system manager
    // SystemManager starts sending KA to its clients as a result of one of two messages:
    // - init message (only CTM uses it)     PsRemoteTask::handle_init_msg
    // - when systemManager receives register_event from its client it start KA PsRemoteTask::set_events
    // HRM doesn't perform any register event and doesn't send init message  => NO KA started by systemManager
    m_systemMgrCli->register_event(EVENT_GRP_GEN_NO_DATA,EVENT_ID_GEN_GOING_ACTIVE,(kpEventCB)&HRM_SystemMgrClient::goingActiveEventDummy);
    NE_DEBUG(DL_INFO,("HRM_Server::_connectToPeer register dummy event "))

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server setHRMState
//                                   
// set new HRM state, notify clients
// -------------------------------------------------------------
void    HRM_Server::setHRMState(HRM_State* st) 
{
    HRM_State *oldSt = m_hrmCurrentState;

    NE_DEBUG(DL_TRACE,("%s - Started (st=%p)",__FUNCTION__,st));

    if(!st) {
        NE_DEBUG(DL_ERROR,("%s - st==NULL",__FUNCTION__));
        return;
    }

    NE_DEBUG(DL_NOTIFY,
             ("%s - Switch state from [%s] to [%s]",
             __FUNCTION__,
             (oldSt!=NULL) ? oldSt->hrmStateName() : "NULL", 
             st->hrmStateName()));
    m_hrmCurrentState = st; 

    // send internal event GOING_ACTIVE/STANDBY when public status is NOT_READY
    if (m_hrmPublicData.hrmHAStatus           == HOST_ACTST_NOT_READY && 
        m_hrmCurrentState->hrmWorkingStatus() != HOST_ACTST_NOT_READY  ) {
        unsigned int evId;
        string evIdStr;
        if (m_hrmCurrentState->hrmWorkingStatus() == HOST_ACTST_STANDBY)
            evId = EVENT_ID_GEN_HOST_GOING_STANDBY;
        else
        {
            PsRunMode runMode = m_hrmCurrentState->getRunMode();

            switch (runMode)
            {
            case PSRUN_MODE_SWITCHOVER:
                evId = EVENT_ID_GEN_HOST_GOING_ACTIVE_SWITCHOVER;
                evIdStr = "EVENT_ID_GEN_HOST_GOING_ACTIVE_SWITCHOVER";
                break;
            case PSRUN_MODE_TAKEOVER:
                evId = EVENT_ID_GEN_HOST_GOING_ACTIVE_TAKEOVER;
                evIdStr = "EVENT_ID_GEN_HOST_GOING_ACTIVE_TAKEOVER";
                break;
            default:
                evId = EVENT_ID_GEN_HOST_GOING_ACTIVE;
                evIdStr = "EVENT_ID_GEN_HOST_GOING_ACTIVE";
                break;
            }
        }

        if(m_pKeeperCli){
            NE_DEBUG(DL_INFO,("%s - send event %s",__FUNCTION__,
                                                    evIdStr.c_str()));
            m_pKeeperCli->send_event(EVENT_GRP_GEN_NO_DATA, evId, NULL, 0);
        }
    }

    // delete the old state
    if (oldSt != NULL) {
        NE_DEBUG(DL_TRACE,
                 ("%s - delete oldSt [%s]",
                 __FUNCTION__,oldSt->hrmStateName()));
        delete oldSt;
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}


//-------------------------------------------------------------
// HRM_Server handleRequest
//                                   
//  Handle client request
// -------------------------------------------------------------
keeperStatus HRM_Server::handleRequest(GeneralMsg& reqMsg, ClientHandler* cliHndl)
{
    keeperStatus st;
    NE_DEBUG(DL_TRACE,("%s - Started (reqMsg.msgType=%d,cliHndl=%p)",__FUNCTION__,reqMsg.msgType(),cliHndl));

    switch(reqMsg.msgType()){
    case  KEEPER_HOST_ACTIVITY:
        NE_DEBUG(DL_TRACE,("%s - KEEPER_HOST_ACTIVITY",__FUNCTION__));
        st = _handlePeerHRMMessage (reqMsg);
        break;

    default:
        st = KeeperSrv::handleRequest(reqMsg, cliHndl);
        break;
    }

    NE_DEBUG(DL_TRACE,("%s - Finished (st=%d)",__FUNCTION__,st));
    return st;
}

//-------------------------------------------------------------
// HRM_Server _handlePeerHRMMessage
//                                   
// handle peer HRM message 
// -------------------------------------------------------------
keeperStatus    HRM_Server::_handlePeerHRMMessage (GeneralMsg& reqMsg)
{
    NE_DEBUG(DL_INFO,("%s - Started (reqMsg.msgType=%d)",__FUNCTION__,reqMsg.msgType()));

    NE_DEBUG(DL_INFO,("%s - cmd %d from slot %d, state %s",
             __FUNCTION__,
             reqMsg.msgHRMCmd(),
             reqMsg.msgHRMData()->hrmSlotNum,
             hostActStatusStr(reqMsg.msgHRMData()->hrmHAStatus)));

    switch(reqMsg.msgHRMCmd()){
    case HRM_CMD_NOTIFY_STATE:
        NE_DEBUG(DL_INFO,("%s - HRM_CMD_NOTIFY_STATE",__FUNCTION__));

        if(m_hrmPeerData.hrmSlotNum != reqMsg.msgHRMData()->hrmSlotNum){
            NE_DEBUG(DL_WARNING,
                     ("%s - Wrong HRM data, slot %d instead of %d - message ignored",
                     __FUNCTION__,
                     reqMsg.msgHRMData()->hrmSlotNum,
                     m_hrmPeerData.hrmSlotNum));
            return KEEPER_FAILURE;
        }
        // check 'hrm configuration' for conflict
        if(m_hrmMode != reqMsg.msgHRMData()->hrmMode)
        {
            NE_DEBUG(DL_INFO,
                     ("%s - Found conflict: Peer HRM mode from slot %d is [%s] - my HRM mode is [%s]",
                     __FUNCTION__,
                     reqMsg.msgHRMData()->hrmSlotNum,
                     hrmMode2Str(reqMsg.msgHRMData()->hrmMode),
                     hrmMode2Str(m_hrmMode) ));

            m_hrmPeerData.hrmHAStatus = reqMsg.msgHRMData()->hrmHAStatus;
            m_hrmPeerData.hrmMode     = reqMsg.msgHRMData()->hrmMode;

            // handle a conflict situation
            _handleHrmModeConflict(reqMsg.msgHRMData()->hrmHAStatus);
        }
        else if (m_hrmPeerData.hrmHAStatus != reqMsg.msgHRMData()->hrmHAStatus) {

            if(m_hrmMode != reqMsg.msgHRMData()->hrmMode)
            {
                // we should never get here
                NE_DEBUG(DL_ERROR,
                                     ("%s - Found conflict: Peer HRM mode from slot %d is [%s] - my HRM mode is [%s]",
                                     __FUNCTION__,
                                     reqMsg.msgHRMData()->hrmSlotNum,
                                     hrmMode2Str(reqMsg.msgHRMData()->hrmMode),
                                     hrmMode2Str(m_hrmMode) ));
                return KEEPER_FAILURE;
            }

            NE_DEBUG(DL_INFO,
                     ("%s - Update Peer HRM status from slot %d - new Peer status is [%s]",
                     __FUNCTION__,
                     reqMsg.msgHRMData()->hrmSlotNum,
                     hostActStatusStr(reqMsg.msgHRMData()->hrmHAStatus)));

            m_hrmPeerData.hrmHAStatus = reqMsg.msgHRMData()->hrmHAStatus;
            m_hrmPeerData.hrmMode     = reqMsg.msgHRMData()->hrmMode;
            
            if(NULL == m_hrmCurrentState)
            {
                // no 'hrm mode' conflict
                // my state is NULL
                // start states
                _initHRMState();
            }

            if(m_hrmCurrentState){
            	m_hrmCurrentState->handleNewPeerHAStatus(reqMsg.msgHRMData());
            }    
        }
        break;

    case HRM_CMD_SET_STATE:  // Request from peer HRM to set a new state.
        NE_DEBUG(DL_INFO,("%s - HRM_CMD_SET_STATE",__FUNCTION__));

        if (m_hrmPublicData.hrmHAStatus != reqMsg.msgHRMData()->hrmHAStatus && m_hrmCurrentState){
            NE_DEBUG(DL_NOTIFY,
                     ("%s - HRM received command to force Host Activity status to [%s]",
                     __FUNCTION__,
                     hostActStatusStr(reqMsg.msgHRMData()->hrmHAStatus)));
            m_hrmCurrentState->forceNewState(reqMsg.msgHRMData()->hrmHAStatus);
        }
        break;

    case HRM_CMD_GET_STATE: // request from systemManager
        NE_DEBUG(DL_INFO,("%s - HRM_CMD_GET_STATE",__FUNCTION__));

        memcpy(reqMsg.msgHRMData(), &m_hrmPublicData, sizeof(HRMData));
        NE_DEBUG(DL_NOTIFY,
                 ("%s - HRM Reply : my slot is %d, my status is %s",
                  __FUNCTION__,
                 reqMsg.msgHRMData()->hrmSlotNum,
                 hostActStatusStr(reqMsg.msgHRMData()->hrmHAStatus)));
        reqMsg.setHRMReplyStatus(KEEPER_SUCCESS);
        break;

    default:
        NE_DEBUG(DL_WARNING,("%s - Unknown HRM Command %d , ignore it",__FUNCTION__,reqMsg.msgHRMCmd()));
        return KEEPER_FAILURE;
        break;
    }


    NE_DEBUG(DL_INFO,("%s - Finished ",__FUNCTION__));
    return KEEPER_SUCCESS;
}

/*!
 * @brief - Restarts the SoftwareReadyTimeout timer.The SoftwareReadyTimeout timer is used by HRM to verify that a card became active during a pre-determined period.
 * If the card fails to do so HRM reboots it (once the card expires).
 * In some cases, we prevent the timer from expiring even though the card didn't become active because the issue the card
 * is experiencing cannot be solved by a reboot. This is done by setting an appropriate flag (see HRM_Server::handleSoftwareReadyTimeout).
 * Once such an issue is resolved, the flag is reset and the timer returns to behave as usual.
 * In such a scenario we need to reset the timer because otherwise it may expire before the card
 * can complete its boot (a user may solve an issue very right before the timer, which is rescheduled
 * automatically while the issue exist, is set to expire).
 *
 * @return - void
 */
void HRM_Server::rescheduleSwReadyTimeoutTimer()
{
    // Delete Timer
    if (m_softwareReadyTimeoutTimer != -1) {
        deleteTimer(m_softwareReadyTimeoutTimer);
        m_softwareReadyTimeoutTimer = -1;
    }

   // Restart timer
    int swReadyTimeout = m_hrmCurrentState->getSoftwareReadyTimeout();
    startSoftwareReadyTimeoutTimer(swReadyTimeout);
}

//---------------------------------------------------------------------------------
// HRM_Server _updateHRMMode
//
// update 'hrm mode' - change hrm.conf file
// --------------------------------------------------------------------------------
bool HRM_Server::_updateHRMMode(HrmMode hrmMode){

    //check if file is initialized
    if ( false == m_configHrmStorage.isInitialized() )
    {
        if (false == m_configHrmStorage.init(HRM_CONF_FILE_NAME) )
        {
            NE_DEBUG(DL_ERROR,
                     ("%s - m_configHrmStorage.init(\"%s\") failed\n",
                     __FUNCTION__,HRM_CONF_FILE_NAME));
            return false;
        }
    }

    // Update HRM mode in hrm.conf file
    if ( m_configHrmStorage.setCfgParamValueInt(CFG_HRM_MODE, hrmMode) != true)
    {
        NE_DEBUG(DL_ERROR,
                     ("%s - cfgHrmStore->setCfgParamValueInt(\"%s\",%d) failed\n",
                     __FUNCTION__,CFG_HRM_MODE,hrmMode));
        return false;
    }

    if ( m_configHrmStorage.saveChanges() != true)
    {
        NE_DEBUG(DL_ERROR,
                     ("%s - cfgHrmStore->saveChanges() failed\n",
                     __FUNCTION__));
        return false;
    }

    NE_DEBUG(DL_INFO,("Set HRM mode to [%s]", (hrmMode == HRM_MODE_ENABLE_E) ? "Enabled" : "Disabled"));

    return true;
}

//---------------------------------------------------------------------------------
// HRM_Server _handleHrmModeConflict
//
// handle 'hrm mode' conflict - when peer has different configuration of 'hrm mode'
// --------------------------------------------------------------------------------
void HRM_Server::_handleHrmModeConflict(HostActivityStatus peerStatus)
{
    NE_DEBUG(DL_TRACE,("%s - Started (peerStatus=%u)",__FUNCTION__,peerStatus));
    
    switch(m_hrmMode){
    case HRM_MODE_ENABLE_E:
        
        // check my state
        // NOTE: if state is not NULL we are already running nothing todo
        if(NULL == m_hrmCurrentState)
        {//my state is NA

            // check peer state
            // If Peer State is Active / Standby
            if(     (HOST_ACTST_ACTIVE  == peerStatus)  ||
                    (HOST_ACTST_STANDBY == peerStatus)  )
            {
                // if peer state is one of the above
                // change hrm mode to disable and reboot
                _updateHRMMode(HRM_MODE_DISABLE_E);
                
                //reboot
                doRebootNeeded(COMMAND_REBOOT);
            }
            else
            {
                // peer state is undefined
                // continue to enable mode
                // run election
                m_hrmCurrentState = new HRM_StateElection(this);
            }
        }
        break;

    case HRM_MODE_DISABLE_E:
        
        // check my state
        // NOTE: if state is not NULL we are already running nothing todo
        if(NULL == m_hrmCurrentState)
        {//my state is NA
            
            // for all peer states: HOST_ACTST_NOT_READY / HOST_ACTST_ACTIVE / HOST_ACTST_STANDBY / HOST_ACTST_NA
            // change my mode to enable and reboot

            // if peer state is one of the above
            // change hrm mode to enable and reboot
            _updateHRMMode(HRM_MODE_ENABLE_E);
            
            //reboot
            doRebootNeeded(COMMAND_REBOOT);
        }
        break;

    default:
        NE_DEBUG(DL_WARNING,
                 ("%s - Unknown m_hrmMode %d - ignore it",__FUNCTION__,m_hrmMode));
        return; // single blades
    }

    NE_DEBUG(DL_TRACE,("%s - Finish",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server sendActivityStatusEvent
//                                   
// notify host activity status
// -------------------------------------------------------------
void    HRM_Server::sendActivityStatusEvent()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));
    
    if (!m_pKeeperCli) {
        NE_DEBUG(DL_WARNING, 
                 ("%s - Cannot send Host Activity Status event, keeper client is not create",
                 __FUNCTION__));
        return;
    }

    if (m_pKeeperCli->get_keeperSock() < 0){
        NE_DEBUG(DL_WARNING, 
                 ("%s - Cannot send Host Activity Status event, keeper client is not init yet",
                 __FUNCTION__));
        return;
    }

    CardInfo card;
    memset(&card, 0, sizeof(card));
    card.slot_num = m_hrmPublicData.hrmSlotNum;
    card.haStatus = m_hrmPublicData.hrmHAStatus;
    card.haTimestamp = m_hrmPublicData.hrmHATimestamp;

    NE_DEBUG(DL_INFO,
             ("%s - send event CARD_HOST_ACT (slot_num=%d,haStatus=%d [%s],haTimeStamp=%ld)",
             __FUNCTION__,
             card.slot_num,
             card.haStatus,
             hostActStatusStr(card.haStatus),
             card.haTimestamp));  
    m_pKeeperCli->send_event(EVENT_GRP_CARD, EVENT_ID_CARD_HOST_ACT, &card, sizeof(card));

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server setPublicHostActivityStatus
//                                   
// set Host activity status and notify to clients
// -------------------------------------------------------------
void    HRM_Server::setPublicHostActivityStatus(HostActivityStatus haSt)
{
    NE_DEBUG(DL_TRACE,("%s - Started (haSt=%d [%s])",__FUNCTION__,haSt,hostActStatusStr(haSt)));

    if (m_hrmPublicData.hrmHAStatus == haSt) {
        NE_DEBUG(DL_TRACE,("%s - Finished : No Change",__FUNCTION__));
        return;
    }

    time_t tmNow = time(0);
    m_hrmPublicData.hrmHATimestamp = tmNow;

    if(haSt == HOST_ACTST_ACTIVE)
        m_hrmPublicData.hrmLAT = tmNow;

    NE_DEBUG(DL_NOTIFY,("%s - Update Host Activity Status: status %d [%s]",__FUNCTION__,haSt,hostActStatusStr(haSt)));
    m_hrmPublicData.hrmHAStatus = haSt;

    sendActivityStatusEvent();

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server sendHRMStateMessage
//                                   
// send HRM state messages to peer
//    - send from HRM_PeerClient::handleOpen and HRM_State CTOR
// -------------------------------------------------------------
keeperStatus        HRM_Server::sendHRMStateMessage (HostActivityStatus haSt)
{
    keeperStatus status;
    NE_DEBUG(DL_TRACE,("%s - Started (haSt=%d [%s])",__FUNCTION__,haSt,hostActStatusStr(haSt)));

    if (!m_hrmPeerCli) {
        NE_DEBUG(DL_NOTIFY, ("%s - Cannot send HRM Message , HRM Peer client is not created yet", __FUNCTION__));
        NE_DEBUG(DL_TRACE,("%s - Finished (KEEPER_FAILURE)",__FUNCTION__));
        return KEEPER_FAILURE;
    }

    if (m_hrmPeerCli->get_keeperSock() < 0){
        NE_DEBUG(DL_NOTIFY, ("%s - Cannot send HRM Message , HRM Peer client is not init yet", __FUNCTION__));
        NE_DEBUG(DL_TRACE,("%s - Finished (KEEPER_FAILURE)",__FUNCTION__));
        return KEEPER_FAILURE;
    }


    HRMData kaData;
    kaData.hrmHAStatus = haSt;
    kaData.hrmSlotNum = m_hrmPublicData.hrmSlotNum;
    kaData.hrmLAT = m_hrmPublicData.hrmLAT;
    kaData.hrmMode = m_hrmPublicData.hrmMode;
    
    NE_DEBUG(DL_INFO,("%s - Send message to peer HRM - my status is [%s]",__FUNCTION__,hostActStatusStr(haSt)));
    status = m_hrmPeerCli->sendHRMMessage(HRM_CMD_NOTIFY_STATE, &kaData);

    NE_DEBUG(DL_TRACE,("%s - Finished (status=%d)",__FUNCTION__,status));
    return status;
}

//-------------------------------------------------------------
// HRM_Server sendHRMCommand
//                                   
// send command to peer HRM
// -------------------------------------------------------------
keeperStatus    HRM_Server::sendHRMCommand (HostActivityStatus haSt)
{
    keeperStatus status;
    NE_DEBUG(DL_INFO,("%s - Started (haSt=%d [%s])",__FUNCTION__,haSt,hostActStatusStr(haSt)));

    if (!m_hrmPeerCli) {
        NE_DEBUG(DL_ERROR, 
                 ("%s - Cannot send HRM Message, HRM Peer client has not been initiated yet",
                 __FUNCTION__));
        NE_DEBUG(DL_TRACE,("%s - Finished (KEEPER_FAILURE)",__FUNCTION__));
        return KEEPER_FAILURE;
    }

    if (m_hrmPeerCli->get_keeperSock() < 0){
        NE_DEBUG(DL_ERROR, 
                 ("%s - Cannot send HRM Message, HRM Peer client has not been initiated yet",
                 __FUNCTION__));
        NE_DEBUG(DL_TRACE,("%s - Finished (KEEPER_FAILURE)",__FUNCTION__));
        return KEEPER_FAILURE;
    }

    HRMData cmdData;
    cmdData.hrmHAStatus = haSt;
    cmdData.hrmSlotNum = m_hrmPublicData.hrmSlotNum;
    cmdData.hrmMode     = m_hrmPublicData.hrmMode;

    NE_DEBUG(DL_INFO, ("%s - Send command to peer HRM - set to %s",__FUNCTION__,hostActStatusStr(haSt)));
    status=m_hrmPeerCli->sendHRMMessage(HRM_CMD_SET_STATE, &cmdData);

    NE_DEBUG(DL_INFO,("%s - Finished (status=%d)",__FUNCTION__,status));
    return status;
}


//-------------------------------------------------------------
// HRM_Server handlePeerUnavailable
//                                   
// handle peer unavailable
//        - invoked by peer disconnection handler / Peer KeepAlive handler
// -------------------------------------------------------------
keeperStatus HRM_Server::handlePeerUnavailable()
{
    NE_DEBUG(DL_TRACE,("%s - Started ",__FUNCTION__));

    m_hrmPeerData.hrmHAStatus = HOST_ACTST_NA;

    if(!m_hrmStopped && m_hrmCurrentState)
        m_hrmCurrentState->handlePeerUnavailable();

    NE_DEBUG(DL_TRACE,("%s - Finished (KEEPER_SUCCESS)",__FUNCTION__));
    return KEEPER_SUCCESS;
}

//-------------------------------------------------------------
// HRM_Server handlePeerConnectivityTimeout
//                                   
// handle peer connectivity timeout
//        - invoked by connect timer
// -------------------------------------------------------------
int HRM_Server::handlePeerConnectivityTimeout(void *cb_data)
{
    NE_DEBUG(DL_TRACE,("%s - Started (cb_date=%p)",__FUNCTION__,cb_data));

    m_hrmPeerData.hrmHAStatus = HOST_ACTST_NA;

    if(!m_hrmStopped && (NULL == m_hrmCurrentState))
        _initHRMState();

    if(!m_hrmStopped && m_hrmCurrentState)
        m_hrmCurrentState->handlePeerConnectivityTimeout();

    NE_DEBUG(DL_TRACE,("%s - Finished TIMER_RUN_ONCE",__FUNCTION__));
    return TIMER_RUN_ONCE; 
}

//-------------------------------------------------------------
// HRM_Server startHRMPeerConnectivity
//                                   
// create HRM peer client and connect
// -------------------------------------------------------------
void    HRM_Server::startHRMPeerConnectivity(int peerConnecttimeout)
{
    NE_DEBUG(DL_TRACE,("%s - Started ",__FUNCTION__));

    m_hrmPeerCli = new HRM_PeerClient(k_reactor, this);

    if (!m_hrmPeerCli) {
        NE_DEBUG(DL_ERROR,("%s - Cannot create peer HRM client instance",__FUNCTION__));
        return;
    }

    m_hrmPeerCli->setFlags(0, false, KEEPER_IO_TIMEOUT, peerConnecttimeout, false);

    char addrStr[32];
    struct in_addr peerIp;

    if (SGVE200 == m_productDetails.hwmodel) {
        // SGVE200(vChassis)
        peerIp = m_peerMgmtIp;
    }
    else {
        if (hal_cardInternalMgmtIPStringGet(m_hrmPeerData.hrmSlotNum, 1, addrStr) == IRC_ERR_FAILED) {
            NE_DEBUG(DL_ERROR,("%s - Failed getting IP address of peer host %d",__FUNCTION__, m_hrmPeerData.hrmSlotNum));
            return;
        }
        if (inet_aton(addrStr, &peerIp) == 0) {   // Convert the string IP to binary format in network byte order.
            NE_DEBUG(DL_ERROR,("%s - Cannot connect to peer HRM, invalid address %s",__FUNCTION__, addrStr));
            return;
        }
    }

    m_hrmPeerTOTimer = k_reactor->addTimer(peerConnecttimeout, this, (NeCallback) &HRM_Server::handlePeerConnectivityTimeout, 0, 0);

    // connect2Server expect the peerIp to be in host byte order.
    m_hrmPeerCli->connect2Server(hal_ntohl(peerIp.s_addr), k_keeper_desc.get_port());

    // send init to server for start KA messages
    m_hrmPeerCli->init();

    NE_DEBUG(DL_TRACE,("%s - Finished ",__FUNCTION__));

}

//-------------------------------------------------------------
// HRM_Server handleSoftwareReadyTimeout
//                                   
// handle software ready timeout 
// -------------------------------------------------------------
int HRM_Server::handleSoftwareReadyTimeout(void *cb_data)
{
    NE_DEBUG(DL_TRACE, ("%s - Started", __FUNCTION__));

    // If DBAgent hasn't finished init, then we should wait
	/*
     * The software ready timeout is used by HRM to check if a card came up
     * successfully (become active) during boot. If the card fails to do so,
     * HRM reboots it to try and fix the problem. However, if an internal port
     * fails during boot, rebooting the card isn't useful because port issues
     * need to be fixed manually.
     *
     * Instead, we wait (by re-setting the timer) until the port issue is resolved
     * and then decide what to do based on the card status (if the card is
     * moved to active timer will be deleted, otherwise, the card will be
     * rebooted once the timer is invoked).
     *
     * This logic is applied only on the designated standby host because if the
     * active host is unable to come up its better to switch over to the
     * standby host (allowing the faulty host to be fixed once it will come up
     * again as standby).
     */
	if ((m_localPortBootDone == false && m_hrmCurrentState->isGoingStandby()) || (!m_DBAgentFinishedProvisioning)) {
		return TIMER_RESCHEDULE;
    }
    if (m_hrmCurrentState)
        m_hrmCurrentState->handleSoftwareReadyTimeout();

    m_softwareReadyTimeoutTimer = -1;
 
    NE_DEBUG(DL_TRACE,("%s - Finished TIMER_RUN_ONCE",__FUNCTION__));
    return TIMER_RUN_ONCE; 
}

//-------------------------------------------------------------
// HRM_Server startSoftwareReadyTimeoutTimer 
//                                   
// handle new SW status from keeper
// -------------------------------------------------------------
keeperStatus HRM_Server::startSoftwareReadyTimeoutTimer(int timeout)
{
    NE_DEBUG(DL_TRACE,("%s - Started (timeout[%d])",__FUNCTION__,timeout));

    m_softwareReadyTimeoutTimer = 
          k_reactor->addTimer(timeout,
                              this, 
                              (NeCallback) &HRM_Server::handleSoftwareReadyTimeout, 
                              0, 
                              0);

    NE_DEBUG(DL_TRACE,("%s - Finished (KEEPER_SUCCESS) ",__FUNCTION__));
    return KEEPER_SUCCESS;
}

//-------------------------------------------------------------
// HRM_Server handleSWStatusUpdate
//                                   
// handle new SW status from keeper
// -------------------------------------------------------------
keeperStatus HRM_Server::handleSWStatusUpdate(int newSwStatus)
{
    NE_DEBUG(DL_INFO,("%s - State (newSwStatus=%d [%s])",
             __FUNCTION__ ,
             newSwStatus,
             __redundancy_status_str[newSwStatus]));

    if(!m_hrmCurrentState) {
       NE_DEBUG(DL_ERROR,
                ("%s - No m_hrmCurrentState - ignore it (newSwStatus=%d [%s])",
                __FUNCTION__ ,
                newSwStatus,
                __redundancy_status_str[newSwStatus]));
       return KEEPER_FAILURE;
    }

    if(m_cardSwStatus == newSwStatus) {
       NE_DEBUG(DL_WARNING,
                ("%s - No change in card status - ignore it (newSwStatus=%d [%s])",
                __FUNCTION__ ,
                newSwStatus,
                __redundancy_status_str[newSwStatus]));
        return KEEPER_NO_CHANGES;
    }

    m_cardSwStatus = static_cast<checkState>(newSwStatus);

    if (m_softwareReadyTimeoutTimer != -1) {
        deleteTimer(m_softwareReadyTimeoutTimer);
        m_softwareReadyTimeoutTimer = -1;
    }

    if (newSwStatus == CHECK_STATE_ACTIVE) {
        m_hrmCurrentState->handleSoftwareReady();
    } else 
    if (newSwStatus == CHECK_STATE_BYPASS) {
        m_hrmCurrentState->handleSoftwareFailure();
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;
}

/*!
 * @brief set local port status from keeper
 *
 * @param [in] bool variable is used for checking whether the local port boot is completed or not
 *
 * @return void
 */
void HRM_Server::setLocalPortBootDone(bool portBootDone)
{
	NE_DEBUG(DL_TRACE,("%s - Started (PORTBOOT=%d)",__FUNCTION__,portBootDone));
    m_localPortBootDone = portBootDone;
}


//-------------------------------------------------------------
// HRM_Server handleReinitCompleted
//                                   
// handle reinit completed from keeper
// -------------------------------------------------------------
keeperStatus    HRM_Server::handleReinitCompleted()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    if (m_cardSwStatus == CHECK_STATE_ACTIVE){
        if (m_softwareReadyTimeoutTimer != -1) {
            deleteTimer(m_softwareReadyTimeoutTimer);
            m_softwareReadyTimeoutTimer = -1;
        }
        m_hrmCurrentState->handleSoftwareReady();
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
    return KEEPER_SUCCESS;
}

//-------------------------------------------------------------
// HRM_Server stopHRMKeepalive
//                                   
// stop timer for sending HRM Keep-Alive messages
// -------------------------------------------------------------
//void    HRM_Server::stopHRMKeepalive()
//{
//    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));
//
//    NE_DEBUG(DL_TRACE, ("%s - HRM_Server::stopHRMKeepalive, delete timer %d",__FUNCTION__,m_hrmKATimer));
//    deleteTimer(m_hrmKATimer);
//
//    if(m_hrmPeerCli){
//        delete m_hrmPeerCli;
//        m_hrmPeerCli = NULL;
//    }
//
//    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
//}
//
//-------------------------------------------------------------
// HRM_Server activatePeerHRM
//                                   
// activate peer HRM 
// -------------------------------------------------------------
void    HRM_Server::activatePeerHRM()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    // send Host Activity status event
    setPublicHostActivityStatus(HOST_ACTST_NOT_READY);

    if(m_hrmPeerData.hrmHAStatus != HOST_ACTST_NA){
    	NE_DEBUG (DL_FATAL, ("**********************************"));
    	NE_DEBUG (DL_FATAL, ("Active host, switch-over initiated"));
    	NE_DEBUG (DL_FATAL, ("**********************************"));
        NE_DEBUG(DL_INFO, ("Problem on Active Host - try to activate Peer Host and reboot blade."));

        // send command 'GoActive' to peer
        sendHRMCommand(HOST_ACTST_ACTIVE);

    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server doRebootNeeded
//                                   
// initiate SW reboot of blade 
// -------------------------------------------------------------
void  HRM_Server::doRebootNeeded (CommandType cmd)
{
    NE_DEBUG(DL_TRACE,("%s - Started (cmd=%d)",__FUNCTION__,cmd));

    if (m_pKeeperCli) {
        NE_DEBUG(DL_INFO,("%s - Call m_pKeeperCli->request_boot(%d) - Ask parent keeper for reboot command",__FUNCTION__,cmd));
        m_pKeeperCli->request_boot(cmd);
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server handlePeerConnected
//                                   
// handle peer HRM connection
// -------------------------------------------------------------
int HRM_Server::handlePeerConnected(void *cbData)
{
    NE_DEBUG(DL_TRACE,("%s - Started (cbData=%p)",__FUNCTION__,cbData));

    // cancel peer connect TO timer
    NE_DEBUG(DL_TRACE, ("%s - delete TO timer %d",__FUNCTION__,m_hrmPeerTOTimer));
    deleteTimer(m_hrmPeerTOTimer);

    // send HRM state message
    if (m_hrmCurrentState)
        sendHRMStateMessage(m_hrmCurrentState->hrmWorkingStatus());
    else
    {
        // at this point it can be that state is not initialized yet
        // message will be used to verify that there is no 'HRM configuration' conflict between HRMs
        sendHRMStateMessage(HOST_ACTST_CONNECTED);
    }

    NE_DEBUG(DL_TRACE,("%s - Finished (TIMER_RUN_ONCE)",__FUNCTION__));
    return TIMER_RUN_ONCE;
}

void HRM_Server::sysMgrTimeoutHandler()
{
	if (m_hrmCurrentState) {
		m_hrmCurrentState->sysMgrTimeoutHandler();
	}
}

//-------------------------------------------------------------
// HRM_Server sendKeeperInit
//                                   
// send global init to keeper 
// -------------------------------------------------------------
void    HRM_Server::sendKeeperInit()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    // send global init to keeper
    if (m_pKeeperCli) {
        NE_DEBUG(DL_INFO,("%s - Call m_pKeeperCli->init",__FUNCTION__));
        m_pKeeperCli->init();
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server _readLAT
//                                   
// read HRM Last Activity Time on init
// -------------------------------------------------------------
void    HRM_Server::_readLAT()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));
    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server _writeLAT
//                                   
// set new HRM Last Activity Time value
// -------------------------------------------------------------
void    HRM_Server::_writeLAT()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));
    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}


//-------------------------------------------------------------
// HRM_Server disableHost
//                                
// disable hostKeeper module
// ------------------------------------------------------------
void HRM_Server::disableHost()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    if (!m_pKeeperCli) {
        NE_DEBUG(DL_ERROR,("%s - m_pKeeperCli == NULL",__FUNCTION__));
        return;
    }

    NE_DEBUG(DL_INFO,("%s - Send command to %s to disable task [%s]", 
             __FUNCTION__,
             AosUtils::Instance()->getKeeperDescription().c_str(), 
             DEFAULT_SEC_HOST_KEEPER));
    m_pKeeperCli->set_task_mode(DEFAULT_SEC_HOST_KEEPER,
                                false /* enabled   */,
                                true  /* apply_now */, 
                                false /* persist   */);

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}


//-------------------------------------------------------------
// HRM_Server enableKeeperCheck
//                                
// enable or disable keeper check
// ------------------------------------------------------------
void HRM_Server::enableKeeperCheck(const char* checkName, bool enable, bool apply_now)
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    if (!m_pKeeperCli) {
        NE_DEBUG(DL_ERROR,("%s - m_pKeeperCli == NULL",__FUNCTION__));
        return;
    }

    NE_DEBUG(DL_INFO,("%s - Send command to %s to set [%s]: enable %d, apply_now %d", 
             __FUNCTION__,
             AosUtils::Instance()->getKeeperDescription().c_str(),
             checkName,
             enable,
             apply_now));
    m_pKeeperCli->set_predefined_check(checkName, enable, apply_now , false /* persist */);

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//-------------------------------------------------------------
// HRM_Server notifyStartInstall
//                                
// notify start installation
// ------------------------------------------------------------
void HRM_Server::notifyStartInstall()
{
    NE_DEBUG(DL_TRACE,("%s - Started",__FUNCTION__));

    if(m_hrmCurrentState){
        NE_DEBUG(DL_INFO,("%s - [%s] handle start installation",
                 __FUNCTION__,
                 m_hrmCurrentState->hrmStateName()));
        m_hrmCurrentState->handleStartInstall();
    } else {
        NE_DEBUG(DL_ERROR, 
                 ("%s - cannot handle start installation, NULL state ptr",
                 __FUNCTION__));
    }

    NE_DEBUG(DL_TRACE,("%s - Finished",__FUNCTION__));
}

//--------------------------------------------------------------
//
// Verify at least one time data sync received 
//
// -------------------------------------------------------------
int HRM_Server::verifyDataSyncReceived(void *cb_data)
{
    struct stat Ready4Sync;
    struct stat syncState;
    static int verifyDataSyncReceivedCount = 0;

    // Request reboot timeout
    if(verifyDataSyncReceivedCount == HRM_VERIFY_DATA_SYNC_RECEIVED_COUNT_LIMIT) {
        NE_DEBUG(DL_ERROR,
                 ("%s [%s] - Call doRebootNeeded(COMMAND_BAD_REBOOT)",
                 __FUNCTION__,m_hrmCurrentState->hrmStateName()));
        doRebootNeeded(COMMAND_BAD_REBOOT);
        NE_DEBUG(DL_TRACE,
                 ("%s [%s] - Finished TIMER_RUN_ONCE",
                  __FUNCTION__,
                  m_hrmCurrentState->hrmStateName()));
        return TIMER_RUN_ONCE;
    }

    NE_DEBUG(DL_TRACE,
             ("%s [%s] - Started (cb_data=%p)",
              __FUNCTION__,
              m_hrmCurrentState->hrmStateName(),
              cb_data));

    // Verify if we already receive data sync from the active host 
    if ((stat(HRMDS_STANDBY_READY4SYNC_DIR , &Ready4Sync) == -1) || 
        (stat(HRMDS_STANDBY_SYNC_STATE_FILE, &syncState ) == -1) || 
        (syncState.st_mtime <= m_starttime                     )  ) {
        NE_DEBUG(DL_TRACE,
                 ("%s [%s] - Finished TIMER_RESCHEDULE",
                  __FUNCTION__,
                  m_hrmCurrentState->hrmStateName()));
                 ++verifyDataSyncReceivedCount;
        return TIMER_RESCHEDULE;
    }

    // Pass to the next state
    NE_DEBUG(DL_NOTIFY,
             ("%s [%s] - switch to state [%s] - At least one time received data sync",
              __FUNCTION__,
              m_hrmCurrentState->hrmStateName(),
              HRM_STATE_NAME_STANDBY));
    setHRMState(new HRM_StateStandby(this));

    // Return to caller 
    verifyDataSyncReceivedCount = 0;
    NE_DEBUG(DL_TRACE,
             ("%s [%s] - Finished TIMER_RUN_ONCE",
              __FUNCTION__,
              m_hrmCurrentState->hrmStateName()));
    return TIMER_RUN_ONCE;
}

//------------------------------------------------------------------------
//
// Start timer for verify at least one time data sync received 
//
//------------------------------------------------------------------------
keeperStatus HRM_Server::startVerifyDataSyncReceivedTimer(int timeout)
{
    NE_DEBUG(DL_TRACE,
             ("%s [%s] - Started (timeout=%d)",
              __FUNCTION__,
              m_hrmCurrentState->hrmStateName(),
              timeout));

    // Start timer 
    k_reactor->addTimer(timeout,
                        this,
                        (NeCallback) &HRM_Server::verifyDataSyncReceived,
                        0,
                        0);

    // Return to caller 
    NE_DEBUG(DL_TRACE,
             ("%s [%s] - Finished (KEEPER_SUCCESS) ",
              __FUNCTION__,
              m_hrmCurrentState->hrmStateName()));
    return KEEPER_SUCCESS;
}

/*!
 * @brief - Returns the configured active SoftwareReadyTimeout value
 *
 * @return - int
 */
int HRM_Server::getUserCfgSoftwareActiveReadyTimeout(void)
{
    return (m_cfgSoftwareReadyActiveTimeout);
}

/*!
 * @brief - Returns the configure standby SoftwareReadyTimeout value
 *
 * @return - int
 */
int HRM_Server::getUserCfgSoftwareStandbyReadyTimeout(void)
{
    return (m_cfgSoftwareReadyStandbyTimeout);
}

