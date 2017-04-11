#ifndef _TRAFFIC_HPP_
#define _TRAFFIC_HPP_

#include "MJCommon.h"
#include <threads/MyThreadPool.h>
#include "json/json.h"
#include "Mysql.h"

#include <map>
#include <set>
#include <list>
#include <vector>
#include <sstream>
#include <algorithm>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <math.h>
#include <assert.h>

#include "datetime.h"
#include "CsvCommon.hpp"

using namespace std;

//读取包含所有源信息的配置文件，配置文件格式同parser配置文件
#define IP "../conf/ip.ini"

class Traffic : public MJ::Worker {//继承Worker是为了并行化
public:
    Traffic(Json::Value data){m_trafficData=data;};
    static bool Init();
    bool csv011(const string &query,const string & qid,string &,string &);
    string csv012ScoreAnalyse(const string &query,const string & qid,string &);


private:
    static MJ::CONFIG m_ips;
    //数据库中取得的参考信息
    static void LoadBaseData(const string &host, const string &db, const string &usr, const string &passwd);
    static tr1::unordered_map<string, vector<string>> m_airlines;
    static tr1::unordered_map<string, double> m_time_zone;
    static tr1::unordered_map<string, double> m_summer_zone;
    static tr1::unordered_map<string, vector<DateTime>> m_summer;
    static tr1::unordered_map<string, string> m_mapinfo;
    static tr1::unordered_map<string, string> m_city_country_map;
    static tr1::unordered_map<string, string> m_city_continent;
    static vector<DateTime> m_24h; //预先加载24个小时


    //用于打印日志,以及检查所请求数据的城市群是否全联通
    tr1::unordered_set<string> m_dest_citys;  //目的地城市列表
    tr1::unordered_set<string> m_depts, m_dests;
    bool CheckLinkfull(tr1::unordered_set<string> names);//


    // 所有矩阵行数一致；同一个矩阵，列可能不一样
    vector<double> m_proportionV;    //比例向量：初始化确定
    vector<vector<double>> m_qualityM;
    // 偏好属性
    bool m_accept_train; //是否接受火车
    bool m_accept_bus; //是否接受大巴
    bool m_accept_ship;//是否接受轮渡
    set<int> m_accept_cabin; //所能接受的舱位
    int m_accept_flight_stop; //所能接受的飞机中转次数
    bool m_accept_dept_early; //是否接受早起
    bool m_accept_dest_late; //是否接受晚到
    set<string>  m_prefer_union; //所偏好的航空组织
    int SetDefaultQualityM();
    int SetQualityM(const Json::Value& debug);
    void SetDefaultPreference();
    int GetPreferencesM(Json::Value &prefer);


    vector<vector<string>> ParseTicketStr(const string &ticket_str);
    string RecoveryTicketStr(const vector<vector<string>> &tickets);
    enum TicketType{FLIGHT, TRAIN, BUS, SHIP};
    enum TimeType{GOOD = 0, ACCEPT, BAD};
    enum TimezoneType{NOTCROSS, CROSS, UNLEGAL};
    vector<vector<double>> GetPropertyM(const vector<vector<string>> &tickets,TicketType type,double & prefer_ratio, double &quality_ratio,Json::Value &cur_ticket,TimezoneType status=CROSS, bool isBasic=false);
    //下列为获取票的属性所用到的辅助函数
    void CheckCrossTimezone(string& ticket_str,TimezoneType & tzt);//暂时只支持基本数据类型;类型私有的枚举类型不能作为返回值!
    double CalDur(const string& timestr1, const string& timestr2);
    double CalDur(const string& timestr1, const string& timestr2, const string& city1, const string& city2);
    bool GetCityDistance(const string& city1,const string& city2,int &dis);
    string GetCategory(vector<vector<string>>& ticket);
    bool CheckTrans(const string& city1,const string& time1,TicketType type1, const string& city2, const string& time2,TicketType type2);
    bool IsIntraArea(const string& dept, const string& dest);
    bool IsCrossContinent(const string& dept, const string& dest);
    bool IsShortTraffic(const string& dept, const string& dest);
    int FlightBadTime(const string& timestr, bool isdept, bool isIntraArea);
    int TrainBadTime(const string& timestr, bool isdept);
    int BadTime(const string& timestr, bool is_dept,bool isInstraArea,TicketType type);
    double GetBestDur(const string& city1,const string& city2);


    //各类票的处理入口
    int ParseTraffic();
    int ParseBasic(Json::Value &,TicketType);
    int ParseInterLine(Json::Value &);
    int ParseRoundTrip(Json::Value&);
    int ParseCombine(Json::Value &);

    //内部处理过程
    int ParseReturnFlights(string &str);
    int ParseMultiFlights(Json::Value &str_list, bool isInterLine=false);
    string ParseCombineStr(const string &combine_str);

    //获取基本票(单程飞机,火车,大巴)的得分情况
    vector<vector<string>> GetScore(const string & ticket_str, TicketType type, bool isBasic=false, bool debug=false);
    int FiltandSortTickets(vector<vector<vector<string>>>& tickets_list, Json::Value &res,TicketType type);//后处理
    bool Acceptable(const string& ticket);
    int GetCostEfficientScore(tr1::unordered_map<string,vector<vector<string>>> &,tr1::unordered_map<string,double>&,Json::Value &);
    double m_price_prefer_ratio;
    int SetBasicCostEfficientScore(Json::Value &basic_data,TicketType type);//设置三种基本数据类型的性价比得分
    //里面只存储3种基本数据类型的票;key 由出发城市，到达城市和出发日期组成;value 有score price和unique_key三个元素
    tr1::unordered_map<string,vector<vector<string>>> m_comparable_tickets_basic;
    //m_cost_efficient_score map,给出了票的性价比得分
    tr1::unordered_map<string,double> m_cost_efficient_score_map_basic;

    //给拼中转票调整性价比得分使用
    tr1::unordered_map<string,vector<vector<string>>> m_comparable_tickets_combine;
    tr1::unordered_map<string,double> m_cost_efficient_score_map_combine;

    //给中转和联程票调整性价比得分使用
    tr1::unordered_map<string,vector<vector<string>>> m_comparable_tickets_multi;
    tr1::unordered_map<string,double> m_cost_efficient_score_map_multi;
    vector<vector<string>> m_dept_ticket_multi;

    //价格调整相关
    double m_price_adjust_ratio;//其值在0~1之间; 0代表保持原价不动，1代表价格调节为平均值
    vector<string> m_price_adjust_tickets;//一张票是一个字符串
    tr1::unordered_map<string,double> m_ticket_adjust_price;
    bool m_price_no_adjust;
    int PriceAdjust(vector<string>& tickets);

    //程序并行化相关函数
    Json::Value m_trafficData;//未处理的tsv003的数据,以及处理后的csv011的数据
    Json::Value m_debug_res;  //debug信息,配合ppScoreView展示
    bool ParseTrafficsParallel(Json::Value &);//拆分数据;并行处理;重组结果
    vector<Json::Value> SplitTsv003Data(Json::Value &);//将Tsv003返回的原始数据拆分,其中combine,twiceCombine,roundTrip,roundTripCombine,interLine各拆为一个子任务,剩下的flight,bus,train则一个城市对为一个子任务;被ParseTrafficsParallel所调用
    //得分归一化在此函数中进行
    //方法:
    //      飞机，火车，大巴，拼中转，二次拼中转都作为单程票，按“出发地+目的地+出发日期分类”
    //      往返，联程，拼往返拆成单程票，参与归一化，最后再组合
    //影响:
    //      会使得分天效果明显好转；但也接近排除了分天不均情况下出更好交通的可能, 但分天好，交通在当天选最好-->good; 分天不好，交通好-->badcase==>归一化可行
    //辅助进行得分的归一化
    tr1::unordered_map<string,double> m_categorizedMaxScore;
    tr1::unordered_map<string,double> m_scoreTimes;
    void MergeResultPre(Traffic* traffic);//将除佳通结果的其他辅助数据合并;
    void MergeResult(Json::Value & mergedResult, Traffic* traffic);//将各个子类的处理结果合并重组,被ParseTrafficsParallel所调用,round_count统计往返票的数量，interline_count统计联程票的数量
    void normalizeBasicTicket(string& ticket,bool flag = false);//将一张基本数据类型的票得分归一化
    virtual int doWork(){ParseTraffic();};

    //期待的票
    tr1::unordered_set<string> m_expectedTickets;
    //打印日志信息
    map<string,string> m_log_info;

    //整个行程的起始日期 比如20180808
    string m_startDate;
public:
    //辅助打印日志
    string m_qid;
    string m_uid;
    string m_csuid;
};
#endif
