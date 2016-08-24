/*
 *   Copyright (C) 2014 Pelagicore AB
 *   All rights reserved.
 */

#include <string>
#include "filegateway.h"

FileGateway::FileGateway()
    : Gateway(ID)
    , m_settings({})
{
}

ReturnCode FileGateway::readConfigElement(const JSonElement &element)
{
    FileSetting setting;
    element.read("path-host", setting.pathInHost);
    element.read("path-container", setting.pathInContainer);
    element.read("create-symlink", setting.createSymlinkInContainer);
    element.read("read-only", setting.readOnly);
    element.read("env-var-name", setting.envVarName);
    element.read("env-var-prefix", setting.envVarPrefix);
    element.read("env-var-suffix", setting.envVarSuffix);

    if (setting.pathInHost.size() == 0) {
        log_error() << "FileGateway config is lacking 'path-host' setting";
        return ReturnCode::FAILURE;
    }

    if (setting.pathInContainer.size() == 0) {
        log_error() << "FileGateway config is lacking 'path-container' setting";
        return ReturnCode::FAILURE;
    }

    m_settings.push_back(setting);
    return ReturnCode::SUCCESS;
}

bool FileGateway::activateGateway()
{
    if (m_settings.size() > 0) {
        for (FileSetting &setting : m_settings) {
            std::string path;

            if (isDirectory(setting.pathInHost)) {
                ReturnCode result = getContainer()->bindMountFolderInContainer(setting.pathInHost
                                                                             , setting.pathInContainer
                                                                             , path
                                                                             , setting.readOnly);
                if (isError(result)) {
                    log_error() << "Could not bind mount folder into container";
                    return false;
                }
            } else {
                ReturnCode result = getContainer()->bindMountFileInContainer(setting.pathInHost
                                                                           , setting.pathInContainer
                                                                           , path
                                                                           , setting.readOnly);
                if (isError(result)) {
                    log_error() << "Could not bind mount file into container";
                    return false;
                }
            }

            if (path.size() == 0) {
               log_error() << "Bind mount failed";
               return false;
            }

            if (setting.envVarName.size() != 0) {
                std::string value = StringBuilder() << setting.envVarPrefix << path << setting.envVarSuffix;
                setEnvironmentVariable(setting.envVarName, value);
            }

            if (setting.createSymlinkInContainer) {
                getContainer()->createSymLink(getContainer()->rootFS() + setting.pathInHost, path);
            }
        }
        return true;
    }

    return false;
}

bool FileGateway::teardownGateway()
{
    return true;
}