#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <string>
#include <set>
#include <fstream>

class Scraper
{
public:
	Scraper();
	void collect_service();
	void collect_availability();
	std::string find_service_with_mininum_at();
	std::string find_service_with_maximum_at();
private:
	int hostname_to_ip(std::string hostname , char* ip);
	bool check_available(std::string service, int64_t& ac_time);
private:
	std::vector<bool> is_available;
	std::vector<std::string> services;
	std::vector<int64_t> access_time;
	std::set<std::pair<int64_t, int>> sorted_access_time;
	int service_num = 0;
};

