/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    main.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file main.cc

/// \mainpage Sourcecode Documentation.
///
/// This documentation was generated using doxygen, you can reproduce it by
/// running "doxygen doxygen.conf" from the mediatomb/doc/ directory.

#include <getopt.h>

#include "spdlog/spdlog.h"

#include "contrib/cxxopts.hpp"

#include <csignal>
#include <mutex>
#include <fstream>

#ifdef SOLARIS
#include <iso/limits_iso.h>
#endif

#include "common.h"
#include "server.h"
#include "content_manager.h"

#define OPTSTR "i:e:p:c:m:f:a:l:P:dhD"

using namespace zmm;
using std::string;

int shutdown_flag = 0;
int restart_flag = 0;
pthread_t main_thread_id;

std::mutex mutex_;
std::unique_lock<std::mutex> lock{mutex_};
std::condition_variable cond;

void log_copyright(const std::shared_ptr<spdlog::logger>& l)
{
    l->info("Gerbera UPnP Media Server version {} - {}", VERSION, DESC_MANUFACTURER_URL);
    l->info("===============================================================================");
    l->info("Gerbera is free software, covered by the GNU General Public License version 2");
    l->info("Copyright 2016-2017 Gerbera Contributors.");
    l->info("Gerbera is based on MediaTomb: Copyright 2005-2010 Gena Batsyan, Sergey Bostandzhyan, Leonhard Wimmer.");
    l->info("===============================================================================");
}

void signal_handler(int signum);

int main(int argc, char** argv, char** envp)
{
#ifdef SOLARIS
    String ld_preload;
    char* preload = getenv("LD_PRELOAD");
    if (preload != NULL)
        ld_preload = String(preload);

    if ((preload == NULL) || (ld_preload.find("0@0") == -1)) {
        printf("Gerbera: Solaris check failed!\n");
        printf("Please set the environment to match glibc behaviour!\n");
        printf("LD_PRELOAD=/usr/lib/0@0.so.1\n");
        exit(EXIT_FAILURE);
    }
#endif

    struct sigaction action;
    sigset_t mask_set;

    bool debug = false;
    bool compile_info = false;
    bool version = false;
    bool help = false;

    cxxopts::Options options(argv[0], "Gerbera UPnP Media Server");
    options.add_options()
        ("D,debug", "Enable debug logging", cxxopts::value<bool>(debug))
        ("i,ip", "IP address to bind to", cxxopts::value<string>(), "IP")
        ("e,interface", "Network interface to bind to", cxxopts::value<string>(), "IFACE")
        ("p,port", "Server Port (LibUPnP only permits values >= 49152)", cxxopts::value<int>(), "PORT")
        ("c,config", "Path to the config file", cxxopts::value<string>(), "CONFIG")
        ("m,home", "Path to the home directory", cxxopts::value<string>(), "PATH")
        ("f,cfgdir", "Path to the directory that is holding the configuration", cxxopts::value<string>(), "PATH")
        ("P,pidfile", "File to hold the process id", cxxopts::value<string>(), "PIDPATH")
        ("a,add", "Add file to Gerbera", cxxopts::value<std::vector<string>>(), "FILE")
        ("l,logfile", "log to specified file", cxxopts::value<string>(), "PATH")
        ("compile-info", "print configuration summary and exit", cxxopts::value<bool>(compile_info))
        ("version", "Print version and exit", cxxopts::value<bool>(version))
        ("h,help", "View this help message", cxxopts::value<bool>(help))
        ;

    try {
        // Do the parse
        options.parse(argc, argv);
    }  catch (const cxxopts::option_not_exists_exception& e) {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Help?
    if (help) {
        std::cout << options.help({"", "Group"}) << std::endl;
        exit(EXIT_SUCCESS);
    }

    // Version?
    if (version) {
        std::cout << "Gerbera " << VERSION << std::endl;
        exit(EXIT_SUCCESS);
    }

    // Compile info?
    if (compile_info) {
        std::cout << "Gerbera " << VERSION << std::endl
        << "Compile info:" << std::endl
        << "-------------" << std::endl
        << COMPILE_INFO << std::endl;
        if(strlen(GIT_BRANCH) > 0 || strlen(GIT_COMMIT_HASH) > 0) {
            std::cout << "Git Branch: " << GIT_BRANCH << std::endl
            << "Git Commit: " << GIT_COMMIT_HASH << std::endl;
        }
        exit(EXIT_SUCCESS);
    }

    // Okay, lets run!
    // Init logging
    auto l = spdlog::stderr_logger_mt("log");

    if (debug) {
        spdlog::set_pattern("%Y-%m-%d %X.%e %l: [%t] %v");
        l->flush_on(spdlog::level::trace);
        l->set_level(spdlog::level::trace);
    } else {
        spdlog::set_pattern("%Y-%m-%d %X %l: %v");
        l->flush_on(spdlog::level::info);
    }

    // Print log header
    log_copyright(l);

    // Write pid if needed
    if (options.count("pidfile")) {
        string pid_file = options["pidfile"].as<string>();

        SPDLOG_TRACE(l, "Writing PID to {}", pid_file.c_str());

        std::ofstream pidfile;
        pidfile.open(pid_file.c_str());
        pidfile << ::getpid();
        if (pidfile.fail()) {
            l->error("Could not write pid file {} : {}", pid_file.c_str(), strerror(errno));
            exit(EXIT_FAILURE);
        }
        pidfile.close();
    }

    String home = options["pidfile"].as<string>().c_str();
    SPDLOG_TRACE(l, "Home passed as '{}'", home.c_str());

    String cfgdir = options["cfgdir"].as<string>().c_str();
    SPDLOG_TRACE(l, "cfgdir passed as '{}'", cfgdir.c_str());

    String config_file = options["config"].as<string>().c_str();
    SPDLOG_TRACE(l, "config passed as '{}'", cfgdir.c_str());

    String interface = options["interface"].as<string>().c_str();
    SPDLOG_TRACE(l, "Interface passed as '{}'", interface.c_str());

    String ip = options["ip"].as<string>().c_str();
    SPDLOG_TRACE(l, "IP passed as '{}'", ip.c_str());

    int port = options["port"].as<int>();
    SPDLOG_TRACE(l, "Port passed as '{}'", port);

    // If home is not given by the user, get it from the environment
    if (!string_ok(config_file) && !string_ok(home)) {

        if (!string_ok(cfgdir)) {
            cfgdir = _(DEFAULT_CONFIG_HOME);
        }

        // Check XDG first
        char *h = getenv("XDG_CONFIG_HOME");
        if (h != nullptr) {
            home = h;
            cfgdir = "gerbera";
        } else {
            // Then look for home
            h = getenv("HOME");
            if (h != nullptr)
                home = h;
        }

        if (!string_ok(home)) {
            l->error("Could not determine users home directory\n");
            exit(EXIT_FAILURE);
        }
    }
    l->info("Using home path: '{}'", home.c_str());

    String prefix;
    char* pref = getenv("MEDIATOMB_DATADIR");
    if (pref != nullptr)
        prefix = pref;
    if (!string_ok(prefix))
        prefix = _(PACKAGE_DATADIR);

    String magic;
    char* mgc = getenv("MEDIATOMB_MAGIC_FILE");
    if (mgc != nullptr)
        magic = mgc;
    if (!string_ok(magic))
        magic = nullptr;

    std::shared_ptr<ConfigManager> configManager = std::make_shared<ConfigManager>(
        config_file, home, cfgdir, prefix, magic, debug, ip, interface, port);

    try {
        configManager->load();
    } catch (const mxml::ParseException& pe) {
        l->error("Error parsing config file: {} line {}:{}",
                 pe.context->location.c_str(),
                 pe.context->line,
                 pe.getMessage().c_str());
        exit(EXIT_FAILURE);
    } catch (const Exception& e) {
        l->error("{}", e.getMessage().c_str());
        exit(EXIT_FAILURE);
    }

    port = configManager->getIntOption(CFG_SERVER_PORT);

    main_thread_id = pthread_self();
    // install signal handlers
    sigfillset(&mask_set);
    pthread_sigmask(SIG_SETMASK, &mask_set, nullptr);

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigfillset(&action.sa_mask);
    if (sigaction(SIGINT, &action, nullptr) < 0) {
        l->error("Could not register SIGINT handler!\n");
    }

    if (sigaction(SIGTERM, &action, nullptr) < 0) {
        l->error("Could not register SIGTERM handler!\n");
    }

    if (sigaction(SIGHUP, &action, nullptr) < 0) {
        l->error("Could not register SIGHUP handler!\n");
    }

    if (sigaction(SIGPIPE, &action, nullptr) < 0) {
        l->error("Could not register SIGPIPE handler!\n");
    }

    Ref<SingletonManager> singletonManager = SingletonManager::getInstance();
    std::shared_ptr<Server> server = std::make_shared<Server>();
    try {
        server->upnp_init();
    } catch (const UpnpException& upnp_e) {

        sigemptyset(&mask_set);
        pthread_sigmask(SIG_SETMASK, &mask_set, nullptr);

        upnp_e.printStackTrace();
        l->error("main: upnp error {}", upnp_e.getErrorCode());
        if (upnp_e.getErrorCode() == UPNP_E_SOCKET_BIND) {
            l->error("Could not bind to socket.\n");
            l->info("Please check if another instance of Gerbera or\n");
            l->info("another application is running on port {}.", port);
        } else if (upnp_e.getErrorCode() == UPNP_E_SOCKET_ERROR) {
            l->error("Socket error.\n");
            l->info("Please check if your network interface was configured for multicast!\n");
            l->info("Refer to the README file for more information.\n");
        }

        try {
            singletonManager->shutdown(true);
        } catch (const Exception& e) {
            l->error("{}", e.getMessage().c_str());
            e.printStackTrace();
        }
        exit(EXIT_FAILURE);
    } catch (const Exception& e) {
        l->error("{}", e.getMessage().c_str());
        e.printStackTrace();
        exit(EXIT_FAILURE);
    }

    // Add files specified as arguments
    if (options.count("add"))
    {
        auto& adds = options["add"].as<std::vector<std::string>>();
        for (const auto& f : adds) {
            try {
                // add file/directory recursively and asynchronously
                l->info("Adding {}", f.c_str());
                ContentManager::getInstance()->addFile(String(f.c_str()), true,
                       true, configManager->getBoolOption(CFG_IMPORT_HIDDEN_FILES));
            } catch (const Exception& e) {
                l->error("Failed to add file {}: {}", f.c_str(),
                    e.getMessage().c_str());
                exit(EXIT_FAILURE);
            }
        }
    }

    sigemptyset(&mask_set);
    pthread_sigmask(SIG_SETMASK, &mask_set, nullptr);

    // wait until signalled to terminate
    while (!shutdown_flag) {
        cond.wait(lock);


        /* FIXME doesnt work correctly anyway
        if (restart_flag != 0) {
            l->info("Restarting Gerbera!\n");
            try {
                server = nullptr;

                singletonManager->shutdown(true);
                singletonManager = nullptr;
                singletonManager = SingletonManager::getInstance();

                try {
                    ConfigManager::setStaticArgs(config_file, home, cfgdir,
                        prefix, magic);
                    ConfigManager::getInstance();
                } catch (const mxml::ParseException& pe) {
                    l->error("Error parsing config file: {} line {}:{}",
                        pe.context->location.c_str(),
                        pe.context->line,
                        pe.getMessage().c_str());
                    l->error("Could not restart Gerbera\n");
                    // at this point upnp shutdown has already been called
                    // so it is safe to exit
                    exit(EXIT_FAILURE);
                } catch (const Exception& e) {
                    l->error("Error reloading configuration: {}",
                        e.getMessage().c_str());
                    e.printStackTrace();
                    exit(EXIT_FAILURE);
                }

                ///  \todo fix this for SIGHUP
                server = Server::getInstance();
                server->upnp_init();

                restart_flag = 0;
            } catch (const Exception& e) {
                restart_flag = 0;
                shutdown_flag = 1;
                sigemptyset(&mask_set);
                pthread_sigmask(SIG_SETMASK, &mask_set, nullptr);
                l->error("Could not restart Gerbera\n");
            }
        }*/
    }

    // shutting down
    int ret = EXIT_SUCCESS;
    try {
        singletonManager->shutdown(true);
    } catch (const UpnpException& upnp_e) {
        l->error("main: upnp error {}", upnp_e.getErrorCode());
        ret = EXIT_FAILURE;
    } catch (const Exception e) {
        e.printStackTrace();
        ret = EXIT_FAILURE;
    }

    l->info("Gerbera exiting. Have a nice day.\n");
    log_close();
    exit(ret);
}

void signal_handler(int signum)
{
    auto l = spdlog::get("log");
    if (main_thread_id != pthread_self()) {
        return;
    }

    if ((signum == SIGINT) || (signum == SIGTERM)) {
        shutdown_flag++;
        if (shutdown_flag == 1)
            l->info("Gerbera shutting down. Please wait...\n");
        else if (shutdown_flag == 2)
            l->info("Gerbera still shutting down, signal again to kill.\n");
        else if (shutdown_flag > 2) {
            l->error("Clean shutdown failed, killing Gerbera!\n");
            exit(1);
        }
    } else if (signum == SIGHUP) {
        restart_flag = 1;
    }

    cond.notify_one();

    return;
}
