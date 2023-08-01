#include <utility>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <queue>
#include <cmath>
// #include "ngraph_app.hpp"

#define PI 3.14159265


int main(int argc, char *argv[]) {
    // std::string delimiter{","};
    // std::string s{""};
    // std::vector<std::string> a = gddi::utils::ssplit(s, delimiter);
    // std::cout << a.size() << std::endl;
    // for (auto &item : a) {
    //     std::cout << item << std::endl;
    // }

    // std::map<std::pair<std::string, std::string>,
    //          std::vector<std::pair<int, int>>> a;

    // std::pair<std::string, std::string> k1("person", "car");
    // std::pair<std::string, std::string> k2("car", "person");
    // std::pair<int, int> v1(0, 1);
    // std::pair<int, int> v2(3, 6);
    // std::pair<int, int> v3(4, 9);
    // std::pair<int, int> v4(5, 6);

    // a[k1].push_back(v1);
    // a[k1].push_back(v2);

    // a[k2].push_back(v3);
    // a[k2].push_back(v4);
    // std::cout << a.size() << std::endl;

    // std::shared_ptr<int> a = std::make_shared<int>(10);
    // auto ap = a.get();
    // std::cout << ap << std::endl;

    // std::queue<int> a;
    // std::cout << a.empty() << std::endl;
    // a.push(1);
    // std::cout << a.size() << " " << a.front() << std::endl;
    // a.pop();
    // std::cout << a.empty() << std::endl;

    std::cout << atan(1 / sqrt(3)) << std::endl;

    std::cout << atan(1) * 180 / PI << std::endl;
    std::cout << atan(-1) * 180 / PI << std::endl;


    return 0;
}
