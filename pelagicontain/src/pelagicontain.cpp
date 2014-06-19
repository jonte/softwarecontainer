/*
 *   Copyright (C) 2014 Pelagicore AB
 *   All rights reserved.
 */
#include <sys/wait.h>
#include <unistd.h>
#include <glibmm.h>

#include "container.h"
#include "pelagicontain.h"

Pelagicontain::Pelagicontain(PAMAbstractInterface *pamInterface,
                             MainloopAbstractInterface *mainloopInterface,
                             ControllerAbstractInterface *controllerInterface,
                             const std::string &cookie):
    m_container(NULL),
    m_pamInterface(pamInterface),
    m_mainloopInterface(mainloopInterface),
    m_controllerInterface(controllerInterface),
    m_cookie(cookie),
    m_launching(false)
{
}

Pelagicontain::~Pelagicontain()
{
    if (m_container) {
        delete m_container;
    }
}

void Pelagicontain::addGateway(Gateway *gateway)
{
//     m_gateways.push_back(gateway);
    m_gateways[gateway->id()] = gateway;
}

// Preload the container. This is a non-blocking operation
pid_t Pelagicontain::preload(const std::string &containerName,
                             const std::string &containerConfig,
                             const std::string &containerRoot,
                             const std::string &containedCommand)
{
    m_container = new Container(containerName, containerConfig, containerRoot);
    Glib::SignalChildWatch cw = Glib::signal_child_watch();

    // Get the commands to run in a separate process
    std::vector<std::string> commands;
    commands = m_container->commands(containedCommand);

    std::string createCommand = commands[0];
    std::string executeCommand = commands[1];
    std::string destroyCommand = commands[2];

    log_debug(createCommand.c_str());
    system(createCommand.c_str());

    log_debug(executeCommand.c_str());
    std::vector<std::string> executeCommandVec;
    executeCommandVec = Glib::shell_parse_argv(executeCommand);

    std::vector<std::string> envVarVec = {"MOUNT_DIR=" + containerRoot + "/late_mounts"};

    int pid;
    try {
        Glib::spawn_async_with_pipes(
            ".",
            executeCommandVec,
            envVarVec,
            Glib::SPAWN_DO_NOT_REAP_CHILD | Glib::SPAWN_SEARCH_PATH,
            sigc::slot<void>(),
            &pid);
    } catch (const Glib::Error &ex) {
        log_error("spawn error: %s", ex.what().c_str());
    }

    sigc::slot<void, int, int> shutdownSlot;
    shutdownSlot = sigc::bind<0>(
        sigc::mem_fun(*this, &Pelagicontain::handleControllerShutdown),
        destroyCommand /* First param to handleControllerShutdown */);
    cw.connect(shutdownSlot, pid);

    return pid;
}

void Pelagicontain::handleControllerShutdown(const std::string lxcExitCommand,
                                                   int pid,
                                                   int exitCode)
{
    log_debug("Controller (pid %d) exited with code: %d. Shutting down now..",
              pid, exitCode);

    log_debug("Issuing: %s", lxcExitCommand.c_str());
    Glib::spawn_command_line_sync(lxcExitCommand);

    shutdownGateways();
    m_pamInterface->unregisterClient(m_cookie);

    log_debug("Queueing up main loop termination");
    Glib::signal_idle().connect(sigc::mem_fun(*this, &Pelagicontain::killMainLoop));
}

void Pelagicontain::launch(const std::string &appId)
{
    log_debug("Launch called with appId: %s", appId.c_str());
    m_launching = true;
    m_appId = appId;
    if (m_container) {
        // this should always be true except when unit-testing.
        m_container->setApplication(appId);
    }
    m_pamInterface->registerClient(m_cookie, m_appId);
}

void Pelagicontain::update(const std::map<std::string, std::string> &configs)
{
    log_debug("update called");
    setGatewayConfigs(configs);

    m_pamInterface->updateFinished(m_cookie);

    activateGateways();

    // We should only start the app if we have ended up here because launch was
    // called and the app has not been started previously.
    if (m_launching && !m_controllerInterface->hasBeenStarted()) {
        m_controllerInterface->startApp();
    }
}

void Pelagicontain::setGatewayConfigs(const std::map<std::string, std::string> &configs)
{
    // Go through the received configs and see if they match any of
    // the running gateways, if so: set their respective config
//     std::string config;
//     std::string gatewayId;
// 
//     for (std::vector<Gateway *>::iterator gateway = m_gateways.begin();
//         gateway != m_gateways.end(); ++gateway)
//     {
//         gatewayId = (*gateway)->id();
//         if (configs.count(gatewayId) != 0) {
//             config = configs.at(gatewayId);
//             (*gateway)->setConfig(config);
//         }
//     }

    // Set configs on only the gateways for which we received a config for
    for (std::map<std::string, std::string>::const_iterator iter = configs.begin();
         iter != configs.end();
         ++iter)
    {
        std::string gatewayId = iter->first;
        if (m_gateways.count(gatewayId) != 0) {
            std::string config = iter->second;
            m_gateways[gatewayId]->setConfig(config);
            m_configured.push_back(gatewayId);
        }
    }
}

void Pelagicontain::activateGateways()
{
//     for (std::vector<Gateway *>::iterator gateway = m_gateways.begin();
//          gateway != m_gateways.end(); ++gateway) {
//         (*gateway)->activate();
//     }

    for (std::vector<std::string>::iterator gatewayId = m_configured.begin();
         gatewayId != m_configured.end();
         ++gatewayId)
    {
        m_gateways[*gatewayId]->activate();
    }
}

void Pelagicontain::setContainerEnvironmentVariable(const std::string &var, const std::string &val)
{
    m_controllerInterface->setEnvironmentVariable(var, val);
}

void Pelagicontain::shutdown()
{
    log_debug("shutdown called");
    // Tell Controller to shut down the app and Controller will exit when the
    // app has shut down and then we will handle the signal through the handler.
    m_controllerInterface->shutdown();
}

void Pelagicontain::shutdownGateways()
{
//     for (std::vector<Gateway *>::iterator gateway = m_gateways.begin();
//          gateway != m_gateways.end(); ++gateway)
//     {
//         if (!(*gateway)->teardown()) {
//             log_warning("Could not tear down gateway cleanly");
//         }
//         delete (*gateway);
//     }
// 
//     m_gateways.clear();

    for (std::vector<std::string>::iterator gatewayId = m_configured.begin();
         gatewayId != m_configured.end();
         ++gatewayId)
    {
        if (!m_gateways[*gatewayId]->teardown()) {
            log_warning() << "Could not tear down gateway cleanly: " << *gatewayId;
        }

        delete m_gateways[*gatewayId];
        m_gateways.clear();
        m_configured.clear();
    }
}
