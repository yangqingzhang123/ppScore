#ifndef _HOTEL_HPP_
#define _HOTEL_HPP_

#include "MJCommon.h"
#include <threads/MyThreadPool.h>
#include "json/json.h"
#include "Mysql.h"

#include <map>
#include <set>
#include <math.h>
#include <list>
#include <algorithm>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "CsvCommon.hpp"
#include "datetime.h"

#include <assert.h>

using namespace std;

//读取包含所有源信息的配置文件，配置文件格式同parser配置文件
#define IP "../conf/ip.ini"

bool hotelComp(const Json::Value &h1, const Json::Value &h2);

class Hotel : public MJ::Worker{//继承Worker是为了并行化处理
public:
    static bool Init();
    string csv010(const string &query,const string & qid,string & other_params,string ptid="");
    string csv020(const string &query,const string & qid,string & other_params,string ptid="");
private:

    static MJ::CONFIG m_ips;
    static void LoadBaseData(const string &host, const string &db, const string &usr, const string &passwd);
    static tr1::unordered_map<string, double> m_dis_map;
    static tr1::unordered_map<string, Json::Value> m_hotel_map;
    static tr1::unordered_map<string, double> m_city_hotel_statis_price_map; //statis 统计性
    static pair<double,double> m_city_map_info;

    int SetDefaultQualityM();
    int SetQualityM(const Json::Value &debug);
    bool GetScores();
    int HotelSelect();


    double m_budget_ratio;
    string m_cid;
    string m_ptid;
    Json::Value m_prefer;
    double m_price_prefer_ratio;
    bool m_star_prefer_13_only;
    map<int,double> m_star_price_prefer_ratios;


    map<string,string> m_log_info;
public:
    //辅助打印日志
    string m_qid;
    string m_uid;
    string m_csuid;
    int m_display_all;
    bool m_isPrivate;
    //所有矩阵行数一致
    vector<double> m_proportionV;
    vector<vector<double>> m_qualityM;

    //程序并行化相关成员
    Json::Value m_hotelData;
    Json::Value m_debug_res;//debug信息，配合ppScoreView展示
    bool GetScoresParallel(Json::Value &);//拆分数据 并行处理， 重组结果
    void MergeResult(Hotel *hotel,Json::Value&);//重组并行化的结果
    virtual int doWork(){GetScores();HotelSelect();}
};

#endif
