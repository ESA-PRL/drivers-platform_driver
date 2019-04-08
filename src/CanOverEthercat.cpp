#include "CanOverEthercat.h"
#include "soem/ethercat.h"

#define EC_TIMEOUTMON 500

int CanOverEthercat::_expected_wkc = 0;
volatile int CanOverEthercat::_wkc = 0;
bool CanOverEthercat::_is_initialized = false;
bool CanOverEthercat::_pdo_update = 0;

CanOverEthercat::CanOverEthercat()
//: _is_initialized(false)
{
    init();
}

CanOverEthercat::CanOverEthercat(const std::string& devName)
//: _is_initialized(false)
{
    init(devName);
}

CanOverEthercat::~CanOverEthercat()
{
    close();
}

void CanOverEthercat::init()
{
}

void CanOverEthercat::init(const std::string& devName)
{
    /* initialise SOEM, bind socket to ifname */
    if (ec_init(devName.c_str()))
    {
       printf("ec_init on %s succeeded.\n", devName.c_str());
       /* find and auto-config slaves */
       if (ec_config_init(FALSE) > 0)
       {
           printf("%d slaves found and configured.\n", ec_slavecount);

           // Set preop to safeop hook
           //for (slc = 1; slc <= ec_slavecount; slc++)
           //{
           //    ec_slave[slc].PO2SOconfig = &ELMOsetup;
           //}

           ec_config_map(&_io_map);
           ec_configdc();

           printf("Slaves mapped, state to SAFE_OP.\n");
           /* wait for all slaves to reach SAFE_OP state */
           ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

           _expected_wkc = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
           printf("Calculated workcounter %d\n", _expected_wkc);

           printf("Request operational state for all slaves\n");
           ec_slave[0].state = EC_STATE_OPERATIONAL;
           /* send one valid process data to make outputs in slaves happy*/
           ec_send_processdata();
           ec_receive_processdata(EC_TIMEOUTRET);
           /* request OP state for all slaves */
           ec_writestate(0);
           int chk = 40;
           /* wait for all slaves to reach OP state */
           do
           {
               ec_send_processdata();
               ec_receive_processdata(EC_TIMEOUTRET);
               ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
           }
           while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));

           if (ec_slave[0].state == EC_STATE_OPERATIONAL)
           {
               printf("Operational state reached for all slaves.\n");
               _is_initialized = true;

               /* create thread for pdo cycle */
               pthread_create(&_thread_handle, NULL, &pdo_cycle, NULL);
           }
           else
           {
               printf("Not all slaves reached operational state.\n");
               _is_initialized = false;

               ec_readstate();

               for (int i = 1; i <= ec_slavecount; i++)
               {
                   if (ec_slave[i].state != EC_STATE_OPERATIONAL)
                   {
                       printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n", i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                   }
               }

               printf("Close socket\n");
               ec_close();
           }

       }
       else
       {
           printf("No slaves found.\n");
           _is_initialized = false;

           printf("Close socket\n");
           ec_close();
       }
    }
    else
    {
        printf("ec_init on %s not succeeded.\n", devName.c_str());
        _is_initialized = false;
    }
}

void CanOverEthercat::close()
{
    if (isInit())
    {
        printf("\nRequest init state for all slaves\n");
        ec_slave[0].state = EC_STATE_INIT;
        /* request INIT state for all slaves */
        ec_writestate(0);

        // cancel pdo_cycle thread
        pthread_cancel(_thread_handle);
        pthread_join(_thread_handle, NULL);

        _is_initialized = false;
    }

    printf("Close socket\n");
    ec_close();
}
bool CanOverEthercat::isInit()
{
    return _is_initialized;
}
int  CanOverEthercat::availableMessages()
{
    return _sdo_messages.size() + _pdo_update;
}
bool CanOverEthercat::transmitMsg(CanMsg CMsg, bool bBlocking)
{
    return false;
}
bool CanOverEthercat::receiveMsg(CanMsg* pCMsg)
{
    return false;
}
bool CanOverEthercat::receiveMsgRetry(CanMsg* pCMsg, int iNrOfRetry)
{
    // not implemented
    return false;
}

void *CanOverEthercat::pdo_cycle(void *ptr)
{
    int currentgroup = 0;

    while (1)
    {
        ec_send_processdata();
        _wkc = ec_receive_processdata(EC_TIMEOUTRET);

        // new pdo data available
        _pdo_update = true;

        while (EcatError) printf("%s", ec_elist2string());

        if ((_wkc < _expected_wkc) || ec_group[currentgroup].docheckstate)
        {
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();

            for (int slave = 1; slave <= ec_slavecount; slave++)
            {
               if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
               {
                  ec_group[currentgroup].docheckstate = TRUE;
                  if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                  {
                     printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
                  {
                     printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     ec_slave[slave].state = EC_STATE_OPERATIONAL;
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state > EC_STATE_NONE)
                  {
                     if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!ec_slave[slave].islost)
                  {
                     /* re-check state */
                     ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                     if (ec_slave[slave].state == EC_STATE_NONE)
                     {
                        ec_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (ec_slave[slave].islost)
               {
                  if(ec_slave[slave].state == EC_STATE_NONE)
                  {
                     if (ec_recover_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!ec_group[currentgroup].docheckstate)
               printf("OK : all slaves resumed OPERATIONAL.\n");
        }

        osal_usleep(10000); // roughly 100 Hz
    }
}