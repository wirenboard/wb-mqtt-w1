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

#include "sysfs_w1.h"

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

const auto WBMQTT_DB_FILE = "/var/lib/wb-homa-w1/libwbmqtt.db";
const auto W1_DRIVER_INIT_TIMEOUT_S = chrono::seconds(5);
const auto W1_DRIVER_STOP_TIMEOUT_S = chrono::seconds(5); // topic cleanup can take a lot of time

int main(int argc, char *argv[])
{
	int rc;
    WBMQTT::TMosquittoMqttConfig mqttConfig {};
    mqttConfig.Id = "wb-w1";
    WBMQTT::TPromise<void> initialized;

    //TMQTTOnewireHandler::TConfig mqtt_config;
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
        //LOG(Error) << "Driver takes too long to initialize. Exiting.";
        printf("Error: W1_DRIVER_INIT_TIMEOUT_S");
        exit(1); 
    });

    /* if handling of signal takes too much time: exit with error */
    WBMQTT::SignalHandling::SetOnTimeout(W1_DRIVER_STOP_TIMEOUT_S, [&]{
        //LOG(Error) << "Driver takes too long to stop. Exiting.";
        printf("Error: W1_DRIVER_STOP_TIMEOUT_S");
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
			poll_interval = stoi(optarg) * 1000;
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }

    auto mqttDriver = WBMQTT::NewDriver(WBMQTT::TDriverArgs{}
        .SetBackend(WBMQTT::NewDriverBackend(WBMQTT::NewMosquittoMqttClient(mqttConfig)))
        .SetId(mqttConfig.Id)
        .SetUseStorage(true)
        .SetReownUnknownDevices(true)
        .SetStoragePath(WBMQTT_DB_FILE)
    );

    mqttDriver->StartLoop();
    WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
        mqttDriver->StopLoop();
        mqttDriver->Close();
    });

    mqttDriver->WaitForReady();


    

	auto time_last_published = steady_clock::now();

    /*try {
        //TGpioDriver gpioDriver(mqttDriver, GetConvertConfig(configFileName));

        /*Utils::ClearMappingCache();

        /*gpioDriver.Start();

        WBMQTT::SignalHandling::OnSignals({ SIGINT, SIGTERM }, [&]{
            gpioDriver.Stop();
            gpioDriver.Clear();
        });

        initialized.Complete();
        WBMQTT::SignalHandling::Wait();
    } catch (const TGpioDriverException & e) {
        LOG(Error) << "FATAL: " << e.what();
        return 1;
    } catch (const WBMQTT::TBaseException & e) {
        LOG(Error) << "FATAL: " << e.what();
        return 1;
    }*/

	//mosqpp::lib_cleanup();

	return 0;
}
