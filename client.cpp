#include <cpr/cpr.h>
#include <iostream>
#include <json.hpp>


int main(int argc, char** argv) {
    auto response = cpr::Get(cpr::Url{"https://api.collegefootballdata.com/games?year=2018&seasonType=regular"});
    auto json = nlohmann::json::parse(response.text);
    std::cout << json.dump(4) << std::endl;
}