#include <getopt.h>

#include "onewire_driver.h"
#include "threaded_runner.h"
#include <wblib/signal_handling.h>
#include <wblib/utils.h>
#include <wblib/wbmqtt.h>

using namespace std;

#define LOG(logger) ::logger.Log() << "[w1] "

WBMQTT::TLogger Error("ERROR: ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::RED);
WBMQTT::TLogger Info("INFO: ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::GREY);
WBMQTT::TLogger Debug("DEBUG: ", WBMQTT::TLogger::StdErr, WBMQTT::TLogger::WHITE, false);

const auto     WBMQTT_DB_FILE            = "/var/lib/wb-homa-w1/libwbmqtt.db";
const auto     W1_DRIVER_INIT_TIMEOUT_S  = chrono::seconds(5);
const auto     W1_DRIVER_STOP_TIMEOUT_S  = chrono::seconds(5); // topic cleanup can take a lot of time
const uint32_t DEFAULT_POLL_INTERVALL_MS = 10000;

namespace
{
    void PrintUsage()
    {
        cout << "Usage:" << endl
             << "  wb-homa-w1 [options]" << endl
             << "Options:" << endl
             << "  -d level     enable debuging output:" << endl
             << "                 1 - 1-Wire only;" << endl
             << "                 2 - mqtt only;" << endl
             << "                 3 - both;" << endl
             << "                 negative values - silent mode (-1, -2, -3))" << endl
             << "  -p port      MQTT broker port (default: 1883)" << endl
             << "  -h IP        MQTT broker IP (default: localhost)" << endl
             << "  -u user      MQTT user (optional)" << endl
             << "  -P password  MQTT user password (optional)" << endl
             << "  -i interval  polling interval, ms (default: " << DEFAULT_POLL_INTERVALL_MS
             << " ms)" << endl;
    }

    void ParseCommadLine(int                           argc,
                         char*                         argv[],
                         WBMQTT::TMosquittoMqttConfig& mqttConfig,
                         uint32_t&                     pollingInterval)
    {
        int debugLevel = 0;
        int c;

        while ((c = getopt(argc, argv, "d:i:h:p:u:P:")) != -1) {
            switch (c) {
            case 'd':
                debugLevel = stoi(optarg);
                break;
            case 'p':
                mqttConfig.Port = stoi(optarg);
                break;
            case 'h':
                mqttConfig.Host = optarg;
                break;
            case 'i':
                pollingInterval = stoul(optarg);
                break;
            case 'u':
                mqttConfig.User = optarg;
                break;
            case 'P':
                mqttConfig.Password = optarg;
                break;

            case '?':
            default:
                PrintUsage();
                exit(2);
            }
        }

        switch (debugLevel) {
        case 0:
            break;
        case -1:
            ::Info.SetEnabled(false);
            break;

        case -2:
            WBMQTT::Info.SetEnabled(false);
            break;

        case -3:
            WBMQTT::Info.SetEnabled(false);
            ::Info.SetEnabled(false);
            break;

        case 1:
            ::Debug.SetEnabled(true);
            break;

        case 2:
            WBMQTT::Debug.SetEnabled(true);
            break;

        case 3:
            WBMQTT::Debug.SetEnabled(true);
            ::Debug.SetEnabled(true);
            break;

        default:
            cout << "Invalid -d parameter value " << debugLevel << endl;
            PrintUsage();
            exit(2);
        }

        if (optind < argc) {
            for (int index = optind; index < argc; ++index) {
                cout << "Skipping unknown argument " << argv[index] << endl;
            }
        }
    }
} // namespace

int main(int argc, char* argv[])
{
    WBMQTT::TPromise<void> initialized;

    WBMQTT::SetThreadName("main");
    WBMQTT::SignalHandling::Handle({SIGINT, SIGTERM});
    WBMQTT::SignalHandling::OnSignals({SIGINT, SIGTERM}, [&] { WBMQTT::SignalHandling::Stop(); });

    /* if signal arrived before driver is initialized:
        wait some time to initialize and then exit gracefully
        else if timed out: exit with error
    */
    WBMQTT::SignalHandling::SetWaitFor(W1_DRIVER_INIT_TIMEOUT_S, initialized.GetFuture(), [&] {
        LOG(Error) << "Driver takes too long to initialize. Exiting.";
        cerr << "Error: W1_DRIVER_INIT_TIMEOUT_S" << endl;
        exit(1);
    });

    /* if handling of signal takes too much time: exit with error */
    WBMQTT::SignalHandling::SetOnTimeout(W1_DRIVER_STOP_TIMEOUT_S, [&] {
        LOG(Error) << "Driver takes too long to stop. Exiting.";
        cerr << "Error: W1_DRIVER_STOP_TIMEOUT_S" << endl;
        exit(2);
    });
    WBMQTT::SignalHandling::Start();

    WBMQTT::TMosquittoMqttConfig mqttConfig{};
    mqttConfig.Id         = "wb-w1";
    uint32_t pollInterval = DEFAULT_POLL_INTERVALL_MS;
    ParseCommadLine(argc, argv, mqttConfig, pollInterval);

    cout << "MQTT broker " << mqttConfig.Host << ':' << mqttConfig.Port << endl;

    auto mqttDriver = WBMQTT::NewDriver(
        WBMQTT::TDriverArgs{}
            .SetBackend(WBMQTT::NewDriverBackend(WBMQTT::NewMosquittoMqttClient(mqttConfig)))
            .SetId(mqttConfig.Id)
            .SetUseStorage(false)
            .SetReownUnknownDevices(true)
            .SetStoragePath(WBMQTT_DB_FILE));

    mqttDriver->StartLoop();
    mqttDriver->WaitForReady();

    try {
        {
            TThreadedPeriodicalRunner r(
                std::unique_ptr<IPeriodicalWorker>(new TOneWireDriverWorker("wb-w1",
                                                                            mqttDriver,
                                                                            ::Info,
                                                                            ::Error,
                                                                            "/sys/bus/w1/devices/")),
                pollInterval,
                "w1 thread",
                ::Info);

            initialized.Complete();
            WBMQTT::SignalHandling::Wait();
        }
        mqttDriver->StopLoop();
        mqttDriver->Close();

    } catch (const exception& e) {
        LOG(Error) << "FATAL: " << e.what();
        return 1;
    }

    return 0;
}
