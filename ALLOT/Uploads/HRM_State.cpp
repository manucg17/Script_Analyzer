 
#include "nedbg.h"
#include "HRM_State.h"
#include "HRM_Server.h"



//////////////////////// abstract class HRM_State /////////////////////////////////////

//-------------------------------------------------------------
// HRM_State   CTOR
// 
// -------------------------------------------------------------
HRM_State::HRM_State(HRM_Server* hrmSrv,const char* _hrmStateName) 
                  : m_hrmSrv(hrmSrv) , m_runMode(PSRUN_MODE_BOOT)
                  , m_hrmStateName(_hrmStateName)
{
    NE_DEBUG(DL_TRACE,("%s [%s] - Started (hrmSrv=%p,_hrmStateName=\"%s\")",__PRETTY_FUNCTION__,hrmStateName(),hrmSrv,_hrmStateName));
    NE_DEBUG(DL_TRACE,("%s [%s] - Finished",__PRETTY_FUNCTION__,hrmStateName()));
}

//-------------------------------------------------------------
// HRM_State   DTOR
// 
// -------------------------------------------------------------
HRM_State::~HRM_State()
{
    NE_DEBUG(DL_TRACE,("%s [%s] - Started ",__PRETTY_FUNCTION__,hrmStateName()));
    NE_DEBUG(DL_TRACE,("%s [%s] - Finished",__PRETTY_FUNCTION__,hrmStateName()));
}

//-------------------------------------------------------------
// HRM_State    _runCmd
//                                   
// run shell command
// -------------------------------------------------------------
bool    HRM_State::_runCmd(const char* cmd, bool run_as_privilege /*=false*/)
{
    NE_DEBUG(DL_TRACE,("%s [%s] - Started (cmd [%s])",__PRETTY_FUNCTION__,hrmStateName(),cmd));

    NE_DEBUG(DL_INFO,("%s [%s] - Call system(\"%s\")",__PRETTY_FUNCTION__,hrmStateName(),cmd));
#if 1 // T.B.D for some reason system api return always -1
    runCmd(cmd, run_as_privilege);
#else

    // Check if fork failed
    if (rc == -1) {
        NE_DEBUG(DL_ERROR,("%s [%s] - fork failed : cmd [%s]",__PRETTY_FUNCTION__,hrmStateName(), cmd));
        NE_DEBUG(DL_TRACE,("%s [%s] - Finished (false)",__PRETTY_FUNCTION__,hrmStateName()));
        return false;
    }

    // Check if command failed 
    if (WIFEXITED(rc) != 0) {
        NE_DEBUG(DL_ERROR,("%s [%s] - cmd [%s] failed, rc %d",__PRETTY_FUNCTION__,hrmStateName(),cmd,WIFEXITED(rc)));
        NE_DEBUG(DL_TRACE,("%s [%s] - Finished (false)",__PRETTY_FUNCTION__,hrmStateName()));
        return false;
    }
#endif

    NE_DEBUG(DL_TRACE,("%s [%s] - Finished (true)",__PRETTY_FUNCTION__,hrmStateName()));
    return true;
}


