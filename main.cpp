#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "dirent.h"
#include <getopt.h>
#include <chrono>

#include <wblib/wbmqtt.h>
#include <wblib/signal_handling.h>
#include "onewire_driver.h"
#include <wblib/utils.h>
#include "log.h"

using namespace std;
#define LOG(logger) ::logger.Log() << "[w1] "


const auto WBMQTT_DB_FILE = "/var/lib/wb-homa-w1/libwbmqtt.db";
const auto W1_DRIVER_INIT_TIMEOUT_S = chrono::seconds(5);
const auto W1_DRIVER_STOP_TIMEOUT_S = chrono::seconds(5); // topic cleanup can take a lot of time

int main(int argc, char *argv[])
{
    WBMQTT::TMosquittoMqttConfig mqttConfig {};
    mqttConfig.Id = "wb-w1";
    WBMQTT::TPromise<void> initialized;

    int poll_interval = 10 * 1000; //milliseconds

    WBMQTT::SetThreadName("main");
    WBMQTT::SignalHandling::Handle({ SIGINT, SIGTERM });
    WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
        WBMQTT::SignalHandling::Stop();
    });

    /* if signal arrived before driver is initialized:
        wait some time to initialize and then exit gracefully
        else if timed out: exit with error
    */
    WBMQTT::SignalHandling::SetWaitFor(W1_DRIVER_INIT_TIMEOUT_S, initialized.GetFuture(), [&]{
        LOG(Error) << "Driver takes too long to initialize. Exiting.";
        cerr << "Error: W1_DRIVER_INIT_TIMEOUT_S" << endl;
        exit(1); 
    });

    /* if handling of signal takes too much time: exit with error */
    WBMQTT::SignalHandling::SetOnTimeout(W1_DRIVER_STOP_TIMEOUT_S, [&]{
        LOG(Error) << "Driver takes too long to stop. Exiting.";
        cerr << "Error: W1_DRIVER_STOP_TIMEOUT_S" << endl;
        exit(2);
    });
    WBMQTT::SignalHandling::Start();

    int c;
    //~ int digit_optind = 0;
    //~ int aopt = 0, bopt = 0;
    //~ char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "c:h:p:i:")) != -1) {
        //~ int this_option_optind = optind ? optind : 1;
        switch (c) {
        //~ case 'c':
            //~ printf ("option c with value '%s'\n", optarg);
            //~ config_fname = optarg;
            //~ break;
        case 'p':
            printf ("option p with value '%s'\n", optarg);
            mqttConfig.Port = stoi(optarg);
            break;
        case 'h':
            printf ("option h with value '%s'\n", optarg);
            mqttConfig.Host = optarg;
            break;
        case 'i':
            printf ("option i with value '%s'\n", optarg);
            poll_interval = stoi(optarg) * 1000;
            if (poll_interval <= 0) {
                printf ("Invalid argument, poll intervall must be greater than zero\n");
                return 1;
            }
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }
    
    auto mqttDriver = WBMQTT::NewDriver(WBMQTT::TDriverArgs{}
        .SetBackend(WBMQTT::NewDriverBackend(WBMQTT::NewMosquittoMqttClient(mqttConfig)))
        .SetId(mqttConfig.Id)
        .SetUseStorage(false)
        .SetReownUnknownDevices(true)
        .SetStoragePath(WBMQTT_DB_FILE)
    );

    mqttDriver->StartLoop();
    WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
        mqttDriver->StopLoop();
        mqttDriver->Close();
    });

    mqttDriver->WaitForReady();

    try {
        TOneWireDriver w1_driver(mqttDriver, poll_interval);

        w1_driver.Start();

        WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
            w1_driver.Stop();
            w1_driver.Clear();
        });

        initialized.Complete();
        WBMQTT::SignalHandling::Wait();
        
    } catch (const TW1SensorDriverException & e) {
        LOG(Error) << "FATAL: " << e.what();
        return 1;
    } catch (const WBMQTT::TBaseException & e) {
        LOG(Error) << "FATAL: " << e.what();
        return 1;
    }

	return 0;
}
