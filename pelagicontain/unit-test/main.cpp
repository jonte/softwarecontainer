/*
 *   Copyright (C) 2014 Pelagicore AB
 *   All rights reserved.
 */
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "log-console.h"
#include "debug.h"

LOG_DEFINE_APP_IDS("PCON", "Pelagicontain");
LOG_DECLARE_CONTEXT(Pelagicontain_DefaultLogContext, "PCON", "Main context");

int main(int argc, char **argv) {
    logging::ConsoleLogOutput logOuput("/dev/null");

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
