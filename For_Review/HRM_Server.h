#ifndef __HRM_SRV_h
#define __HRM_SRV_h

#ifdef WIN32
#pragma warning(disable: 4786)
#endif


#include "keeperSrv.h"
#include "HRM_ServerINI.h"
#include "HRM_ClientHandler.h"
#include "acprof.h"
#include "time.h"

#define HRM_MASTER_SLOT_1    1
#define HRM_MASTER_SLOT_7    7

#define HRM_LAT_FILENAME    SWG_DYNAMIC"/hrm.lat"

class keeperClient;
class HRM_State;
class HRM_PeerClient;
class HRM_SystemMgrClient;

//////////////////////// class HRM_Server ////////////////////////////////////////

class HRM_Server: public KeeperSrv
{
    HrmMode                m_hrmMode;  // enabled, disabled.
    int                    m_hrmMasterSlotNum;
    HostActivityStatus    m_forceStatusCfg;

    HRMData                m_hrmPublicData;
    HRMData                m_hrmPeerData;  // This data is maintained through messages between the HRMs.
    struct in_addr         m_peerMgmtIp;   // Holds the peer host IP in network byte order (relevant for SGVE200 only).
    ConfigVersion        m_productDetails;

    HRM_State*        m_hrmCurrentState;
    int                m_hrmPeerTOTimer;
    int                m_softwareReadyTimeoutTimer;
    int                m_cfgSoftwareReadyActiveTimeout; // User configured active card timeout in minutes configured in /opt/allot/conf/hrm.conf
    int                m_cfgSoftwareReadyStandbyTimeout; // User configured standby card timeout in minutes configured in /opt/allot/conf/hrm.conf
    HRM_PeerClient*    m_hrmPeerCli;
    keeperClient*    m_pKeeperCli;

    HRM_SystemMgrClient*    m_systemMgrCli;

    checkState      m_cardSwStatus;

    bool			m_localPortBootDone;
    bool            m_hrmStopped;
    bool            m_DBAgentFinishedProvisioning;

    SysConfStorage  m_configHrmStorage;

public:
    HRM_Server(NeEngine *reactor, keeperClient* pKeeperCli);
    virtual ~HRM_Server();

    // handle client request
    virtual keeperStatus    handleRequest    (GeneralMsg& reqMsg, ClientHandler* cliHndl);

    // HRM server initialization
    virtual keeperStatus    _init_specific ();

    // initialize HRM connection to peer
    void                    _connectToPeer();

    // handle the case when peer has different 'hrm mode'
    void                    _handleHrmModeConflict(HostActivityStatus peerStatus);

    // initiate SW reboot of blade 
    virtual void            doRebootNeeded    (CommandType cmd);

    // set new HRM state
    void    setHRMState(HRM_State* st);

    // set Host activity status and notify to clients
    void    setPublicHostActivityStatus(HostActivityStatus haSt);

    // create HRM peer client and connect
    void    startHRMPeerConnectivity(int peerConnecttimeout);

    // activate peer HRM 
    void    activatePeerHRM();

    // handle peer HRM connection
    int    handlePeerConnected(void*  cbData = 0);

    // send command to peer HRM
    keeperStatus    sendHRMCommand (HostActivityStatus haSt);

    // sending HRM state messages
    keeperStatus    sendHRMStateMessage (HostActivityStatus haSt);

    // handle new SW status from keeper
    keeperStatus    handleSWStatusUpdate(int newSwStatus);
	
    void    setLocalPortBootDone(bool portBootDone);

    // Set to true when DBAgent sends the finished provisioning event
    void setDBAgentFinishedProvisioning(bool finished) { m_DBAgentFinishedProvisioning = finished; };

    // handle software ready timeout
    int          handleSoftwareReadyTimeout(void *cb_data);

    // Restart software ready timeout
    void rescheduleSwReadyTimeoutTimer();

    // start software ready timeout
    keeperStatus startSoftwareReadyTimeoutTimer(int timeout);

    // Get user configured software active ready timeout
    int getUserCfgSoftwareActiveReadyTimeout();

    // Get user configured software standby ready timeout
    int getUserCfgSoftwareStandbyReadyTimeout();

    // handle reinit completed from keeper
    keeperStatus    handleReinitCompleted();

    // handle HRM mode update event from systemManager
    keeperStatus    handleHRMModeUpdate(int newHRMMode);

    // notify host activity status
    void    sendActivityStatusEvent();

    // notify start installation
    void    notifyStartInstall();

    // return slot number
    int        mySlotNum() { return m_hrmPublicData.hrmSlotNum; }

    // return pointer to public HRM data
    const HRMData*    getHRMPublicDataPtr() { return &m_hrmPublicData; }

    // handle peer unavailable - close peer connection / not KeepAlive from peer
    keeperStatus        handlePeerUnavailable();

    // handle peer connectivity timeout 
    int                 handlePeerConnectivityTimeout(void*  cbData = 0);

	// Handle KA timeout between HRM and system manager.
	void sysMgrTimeoutHandler();

    // send global init to keeper
    void    sendKeeperInit();

    // set flag from ini
    void setForceStatusCFg(HostActivityStatus forceCfg) { m_forceStatusCfg = forceCfg; }

    // disable hostKeeper module
    void disableHost();

    // enable or disable keeper check
    void enableKeeperCheck(const char* checkName, bool enable, bool apply_now);

    // handle stop by keeper
    void stop();

    // stop NeEngine loop on timer
    int    stopReactorCb   (void* /* cbData = 0 */);

    // stop Server on timer
    int    stopCb   (void* /* cbData = 0 */);

    // callback to notify start installation via keeperMgr
    static    keeperStatus    notifyStartInstall (void *pData, void* context);

    //time stamp for HRM start
    time_t m_starttime;

    // verify at least one time we received data sysn from active host
    int verifyDataSyncReceived(void *cb_data);

    // start timer to verify at least one time we received data sysn from active host
    keeperStatus startVerifyDataSyncReceivedTimer(int timeout);
protected:

    virtual void        create_iniparser(char* ini_file)
                    { k_ini = new HRM_ServerINI (ini_file, this, k_timer); }

    virtual void            create_client_handler_factory()
    {    k_clientFactory = new HRM_ClientHandlerFactory (k_reactor, this); }

    // init HRM state
    void    _initHRMState();

private:

    // handle peer HRM message 
    keeperStatus    _handlePeerHRMMessage (GeneralMsg& reqMsg);

    // read HRM Last Activity Time on init
    void            _readLAT();

    // set new HRM Last Activity Time value
    void            _writeLAT();

    // update 'hrm mode' - change hrm.conf file
    bool            _updateHRMMode(HrmMode hrmMode);
};



#endif //__HRM_SRV_h

