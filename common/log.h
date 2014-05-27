/*
 *   Copyright (C) 2014 Pelagicore AB
 *   All rights reserved.
 */
#pragma once

#include "pelagicore-log.h"
#include <glibmm.h>

template<typename LogDataType, typename =
		 typename std::enable_if<std::is_base_of<logging::LogDataCommon, LogDataType>::value>::type>
LogDataType& operator<<(LogDataType& log, const Glib::ustring& s) {
	log << s.c_str();
	return log;
}