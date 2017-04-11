#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <stdlib.h>
#include "/home/liuyuan/RealtimeSpider/master/master.hpp"
#include "/home/liuyuan/RealtimeSpider/json/json.h"
using namespace std;

int main(int argc, const char *argv[])
{
    time_t t;
    t = time(NULL);
    cout << t << endl;

    Json::Value value1, value2,value;
    value1["flight_no"]="su201";
    value1["min_price"]=201;
    cout << value1.size() << endl;
    value2["flight_no"]="su202";
    value2["min_price"]=202;
    Json::Value value3 = Json::Value(Json::arrayValue);
    value3.append(value1);
    value3.append(value2);
    value["result"] = value3;
    int i = 0;
    cout << "111" << (value["result"][i])["flight_no"].asString() << endl;;
    cout << "222" << (value["result"][i])["min_price"].asInt() << endl;;
    Json::FastWriter jfw;
    string str = jfw.write(value);
    cout << "value = " << str << endl;
    
    Json::Reader reader;
    Json::Value wawa;
    reader.parse(str, wawa);
    str = jfw.write(wawa);
    cout << "wawa:" << str << endl;

    system("sleep 3");
    time_t e;
    e = time(NULL);

    time_t d;
    d = e - t;
    cout << d << endl;

    return 0;
}
