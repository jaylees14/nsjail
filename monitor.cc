/*

   nsjail - process monitoring
   -----------------------------------------

   Copyright 2017 Google Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#include "monitor.h"

#include <fcntl.h>
#include <chrono>
#include <cstring>
#include <thread>

#include "logs.h"
#include "util.h"

namespace monitor {

int MAX_BUFFER_SIZE = 512;

void monitorMemory(monitorconf_t* monitor_config) {
	// get cgroup path for the pid
	std::string usages;
	for (const auto& metric : monitor_config->metrics) {
		usages += metric + ", ";
	}

	LOG_I("Monitoring usage of %s every %dms - placing results in %s", usages.c_str(),
	    monitor_config->sample_time_ms, monitor_config->output_file_path.c_str());

	char cgroup_name_buffer[MAX_BUFFER_SIZE];
	util::readFromFile(util::StrPrintf("/proc/%d/cgroup", monitor_config->pid).c_str(),
	    cgroup_name_buffer, MAX_BUFFER_SIZE);

	std::vector<std::string> cgroup_names = util::strSplit(cgroup_name_buffer, '\n');

	// remove leading 0:: from cgroup path
	std::string path = std::string(cgroup_names[0]).substr(3);

	std::string cgroup_base = monitor_config->cgroup_mount + path + "/";

	for (const auto& metric : monitor_config->metrics) {
		util::writeBufToFile(monitor_config->output_file_path.c_str(), metric.c_str(),
		    metric.size(), O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);
		util::writeBufToFile(monitor_config->output_file_path.c_str(), ",", 1,
		    O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);
	}
	util::writeBufToFile(monitor_config->output_file_path.c_str(), "\n", 1,
	    O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);

	while (!monitor_config->should_finish) {
		char current_metric[MAX_BUFFER_SIZE];

		for (const auto& metric : monitor_config->metrics) {
			auto read = util::readFromFile(
			    (cgroup_base + metric).c_str(), current_metric, MAX_BUFFER_SIZE);
			// Escape metric with quotes, in the case it spans multiple lines
			util::writeBufToFile(monitor_config->output_file_path.c_str(), "\"", 1,
			    O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);
			util::writeBufToFile(monitor_config->output_file_path.c_str(),
			    current_metric, read, O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);
			util::writeBufToFile(monitor_config->output_file_path.c_str(), "\",", 2,
			    O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);
		}

		util::writeBufToFile(monitor_config->output_file_path.c_str(), "\n", 1,
		    O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC);

		std::this_thread::sleep_for(
		    std::chrono::milliseconds(monitor_config->sample_time_ms));
	}
}

void finishMonitoring(monitorconf_t* monitor_config) {
	monitor_config->should_finish = true;
}

}  // namespace monitor
