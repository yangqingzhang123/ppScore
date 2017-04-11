#include "Traffic.hpp"
using namespace boost;
using namespace MJ;

const double score_total=10.0;
const double quality_score_total=8.0;
const double score_pow=3;//比较性价比时，偏好得分的幂系数

const double price_prefer_ratio_economic=0.9;
const double price_prefer_ratio_comfortable=0.5;
const double price_prefer_ratio_lightLuxury=0.3;
const double price_prefer_ratio_luxury=0.1;

//静态变量初始化
CONFIG Traffic:: m_ips;
tr1::unordered_map<string, vector<string>> Traffic:: m_airlines;
tr1::unordered_map<string, double> Traffic:: m_time_zone;
tr1::unordered_map<string, double> Traffic:: m_summer_zone;
tr1::unordered_map<string, vector<DateTime>> Traffic:: m_summer;
tr1::unordered_map<string, string> Traffic:: m_mapinfo;
tr1::unordered_map<string, string> Traffic:: m_city_country_map;
tr1::unordered_map<string, string> Traffic:: m_city_continent;
vector<DateTime> Traffic:: m_24h; //预先加载24个小时

bool Traffic::Init()
{
    //读取配置文件
    ConfigParser config;
    m_ips = config.read(IP);
    //m_proportion ＝config.read(Proportion);
    //读取数据库
    LoadBaseData(m_ips["base_data"]["ip"], m_ips["base_data"]["db"], m_ips["base_data"]["usr"], m_ips["base_data"]["passwd"]);


    m_24h.clear();
    for (int i = 0; i < 24; ++i){
        string tmp;
        if (i < 10){
            tmp = "0" + boost::lexical_cast<string>(i);
        }else{
            tmp = boost::lexical_cast<string>(i);
        }
        m_24h.push_back(DateTime::Parse("20000101_" + tmp + ":00", "yyyyMMdd_HH:mm"));
    }

    return true;
}

void Traffic::LoadBaseData(const string &host, const string &db, const string &usr, const string &passwd)
{
    Mysql mysql(host, db, usr, passwd);
    if (!mysql.connect()){
        assert(false);
    }

    // get m_airlines  航空公司信息  1－顶级航空2－优质航空3－一般航空4－廉价航空
    if (mysql.query("select code, grade, `union` from airlines;")){
        MYSQL_RES *res = mysql.use_result();
        MYSQL_ROW row;
        if(res){
            while (row = mysql.fetch_row(res)){
                vector<string > tmp;
                tmp.push_back(row[1]);
                tmp.push_back(row[2]==NULL?"":row[2]);
                m_airlines[row[0]] = tmp;
            }
        }
        mysql.free_result(res);
    }
    assert(!m_airlines.empty());

    // 城市国家，时区, 坐标信息 get m_city_country_map
    if (mysql.query("select id, country, time_zone, map_info,summer_zone, summer_start,summer_end,summer_start_next_year,summer_end_next_year,continent from city where map_info is not null;")){
        MYSQL_RES *res = mysql.use_result();
        MYSQL_ROW row;
        if(res){
            while (row = mysql.fetch_row(res)){
                m_city_country_map[row[0]] = row[1];
                m_city_continent[row[0]] = row[9];
                m_time_zone[row[0]] = boost::lexical_cast<double>(row[2]);
                m_summer_zone[row[0]] = boost::lexical_cast<double>(row[4]);
                if (row[5] && row[6] && row[7] && row[8]){
                    vector<DateTime> summer;
                    string t0(row[5]), t1(row[6]), t2(row[7]), t3(row[8]);
                    t0 = t0.size() == 19 ? t0 : "2100-01-02T00:00:00";
                    t1 = t1.size() == 19 ? t1 : "2000-01-02T00:00:00";
                    t2 = t2.size() == 19 ? t2 : "2100-01-02T00:00:00";
                    t3 = t3.size() == 19 ? t3 : "2000-01-02T00:00:00";
                    summer.push_back(DateTime::Parse(t0, "yyyy-MM-ddTHH:mm:ss"));
                    summer.push_back(DateTime::Parse(t1, "yyyy-MM-ddTHH:mm:ss"));
                    summer.push_back(DateTime::Parse(t2, "yyyy-MM-ddTHH:mm:ss"));
                    summer.push_back(DateTime::Parse(t3, "yyyy-MM-ddTHH:mm:ss"));
                    m_summer[row[0]] = summer;
                }

                if(row[3]!=(string)"NULL")
                    m_mapinfo[row[0]] = row[3];
            }
        }
        mysql.free_result(res);
    }
    assert(!m_city_country_map.empty() && !m_time_zone.empty() && !m_summer_zone.empty() && !m_summer.empty());
}

string GetUniqueKey(vector<vector<string>>& ticket)
{
    if(ticket[0].size()>=6)
    {
        return ticket[0][1]+"|"+ticket[0][3]+"|"+ticket[0][4]+"|"+ticket[0][5]+"|"+ticket[1][0]+"|"+ticket[ticket.size()-1][1];

    }
    else
    {
        return ticket[0][1]+"|"+ticket[0][3]+"|"+ticket[0][4]+"|"+ticket[1][0]+"|"+ticket[ticket.size()-1][1];

    }
}

//从basic获得票据分类
string Traffic:: GetCategory(vector<vector<string>>& ticket)
{
    vector<string> dept_place,dest_place;
    boost::split(dept_place,ticket[1][2],boost::is_any_of("#"));
    boost::split(dest_place,ticket.back()[3],boost::is_any_of("#"));
    if(dept_place.size()<2 or dest_place.size()<2 or dept_place[1].empty() or dest_place[1].empty())
    {
        ticket[0][2]=boost::lexical_cast<string>(0.0);//起止段没有归属城市为异常情况，将得分置为0
        return "";
    }
    else
    {
        string dept_name=dept_place[1];
        string dest_name=dest_place[1];
        string deptDate=ticket[1][0].substr(0,8);
        string am_pm = ticket[1][0].substr(9,2);
        if(am_pm<"14") am_pm="am"; else am_pm="pm";
        if(IsCrossContinent(dept_name,dest_name)) am_pm="ap";
        return dept_name+"_"+dest_name+"_"+deptDate+"_"+am_pm;
    }
}

bool CheckScore(double & score)
{
    score=score<(score_total-eps)?score:score_total-eps;
    return true;
}

bool Traffic::csv011(const string &query, const string & qid, string & other_params, string &res)
{
    m_price_no_adjust=false;

    m_log_info.insert(pair<string,string>("qid",qid));
    m_log_info.insert(pair<string,string>("rq_type","csv011"));

    Json::Reader reader;
    Json::FastWriter fastWriter;

    SetDefaultQualityM();
    SetDefaultPreference();

    //解析请求参数
    Json::Value queryobj;
    reader.parse(query, queryobj);
    m_startDate=queryobj.get("startDate","").asString();

    double price_adjust_ratio = queryobj.get("price_adjust_ratio",0.2).asDouble();
    if(price_adjust_ratio<0.0) price_adjust_ratio=0.0;
    else if(price_adjust_ratio>1.0) price_adjust_ratio=1.0;
    m_price_adjust_ratio=price_adjust_ratio;

    m_dest_citys.clear();
    m_depts.clear();
    m_dests.clear();
    vector<string> dataType;
    dataType.push_back("data");
    dataType.push_back("dataRound");
    dataType.push_back("dataHeadTail");
    for(int i=0;i<dataType.size();i++)
    {
        if(queryobj.isMember(dataType[i]))
        {
            vector<string> tmp;
            string str=queryobj[dataType[i]].asString();
            split( tmp, str,is_any_of("-;:#|"));
            for(int j=0;j<tmp.size();j++)
                if(tmp[j].size()==5)
                    m_dest_citys.insert(tmp[j]);
        }
    }

    //判断是否有往返和联程
    int is_rq_interline = 0, is_rq_round = 0;
    m_log_info.insert(pair<string,string>("is_rq_round","0"));
    m_log_info.insert(pair<string,string>("is_rq_interline","0"));
    if(queryobj.isMember("dataRound"))
    {
        is_rq_round=1;
        m_log_info["is_rq_round"] = "1";
    }
    if(queryobj.isMember("dataHeadTail"))
    {
        is_rq_interline=1;
        m_log_info["is_rq_interline"] = "1";
    }
    _INFO("%d, %d", is_rq_round, is_rq_interline);

    //判断是否是debug请求, debug中可能会传入属性比例
    bool debug = false;
    m_debug_res.clear();
    if (queryobj.isMember("debug")){
        debug = true;
        SetQualityM(queryobj["debug"]);
    }

    //debug=1 要全量数据
    queryobj["debug"]=1;

    //cout_matrix(m_qualityM);
    if (queryobj.isMember("prefer")){
        GetPreferencesM(queryobj["prefer"]);
    }
    if (queryobj.isMember("expectedTickets") and queryobj["expectedTickets"].isArray()){
        m_expectedTickets.clear();
        Json::Value md5s=queryobj["expectedTickets"];
        for(size_t i=0; i<md5s.size(); i++)
        {
            if(md5s[i].isString() and not md5s[i].asString().empty()) m_expectedTickets.insert(md5s[i].asString());
        }
    }

    string new_query = fastWriter.write(queryobj);

    //转发请求
    string host = "10.10.135.140:"+m_ips["inner_proxy"]["port"];
    int timeout = 10000000;
    string real_req = "?type=tsv003&qid="+qid+"&query=" + MJ::UrlEncode(new_query)+"&"+other_params;

    SocketClient *client = new SocketClient();
    ServerRst server_rst;
    client->init(host, timeout);
    DateTime t1 = DateTime::Now();
    client->getRstFromHost(real_req, server_rst, 0);
    DateTime t2 = DateTime::Now();
    res = server_rst.ret_str;
    delete client;

    //处理
    Json::Value res_json;
    reader.parse(res, res_json);

    Json::Value &data = res_json["data"];
    Json::Value error = res_json["error"];

    m_log_info.insert(pair<string,string>("is_success","1"));
    if (error["error_id"].asInt() != 0){
        _INFO("fail");
        m_log_info["is_success"] = "0";
        logInfo2Str(m_log_info);
        return false;
    }
    ParseTrafficsParallel(data);
    res_json["data"]=m_trafficData;
    // 重组结果返回

    if(debug)
    {
        res_json["debug_info"]=m_debug_res;
    }
    res = fastWriter.write(res_json);
    // 返回结果
    DateTime t3 = DateTime::Now();

    string connectDetail;
    connectDetail+="citys:";
    for (tr1::unordered_set<string>::iterator it=m_dest_citys.begin(); it!=m_dest_citys.end(); ++it) {connectDetail+="#";connectDetail+=*it;}
    connectDetail+="|dept:";
    for (tr1::unordered_set<string>::iterator it=m_depts.begin(); it!=m_depts.end(); ++it) {connectDetail+="#";connectDetail+=*it;}
    connectDetail+="|dest:";
    for (tr1::unordered_set<string>::iterator it=m_dests.begin(); it!=m_dests.end(); ++it){connectDetail+="#";connectDetail+=*it;}
    _INFO("connectDetail:%s",connectDetail.c_str());

    m_log_info.insert(pair<string,string>("link_full","0"));
    if (CheckLinkfull(m_depts) && CheckLinkfull(m_dests)){
        _INFO("success");
        m_log_info["link_full"] = "1";
    }
    double getDataCost=(t2-t1).GetTotalSeconds();
    _INFO("%f,%f",getDataCost, (t3-t2).GetTotalSeconds());
    string rq_consume = boost::lexical_cast<string>(getDataCost);
    string process_consume = boost::lexical_cast<string>((t3-t2).GetTotalSeconds());
    m_log_info.insert(pair<string,string>("rq_consume",rq_consume));
    m_log_info.insert(pair<string,string>("process_consume",process_consume));
    if(getDataCost >2.0) logException("ex21100",m_uid.c_str(),m_csuid.c_str(),m_qid.c_str(),(string("time cost ")+ boost::lexical_cast<string>(getDataCost)).c_str());

    logInfo2Str(m_log_info);
    return true;
}


//直接传入交通请求结果，然后解析
string Traffic::csv012ScoreAnalyse(const string &query,const string & qid,string & other_params)
{
    m_price_no_adjust=true;

    Json::Reader reader;
    Json::FastWriter fastWriter;

    SetDefaultQualityM();
    SetDefaultPreference();

    //解析请求参数
    Json::Value data;
    reader.parse(query, data);
    m_debug_res.clear();
    m_trafficData=data;
    ParseTraffic(); // 解析结果，拆分出各个交通
    return fastWriter.write(m_debug_res);

}


int Traffic::ParseTraffic()
{
    m_comparable_tickets_basic.clear();
    m_cost_efficient_score_map_basic.clear();

    if(m_trafficData.isMember("flight")){
        _INFO("flight");
        ParseBasic(m_trafficData["flight"],FLIGHT);
        _INFO("flight end");
    }
    if (m_trafficData.isMember("bus")){
        _INFO("bus");
        ParseBasic(m_trafficData["bus"],BUS);
        _INFO("bus end");
    }
    if (m_trafficData.isMember("train")){
        _INFO("train");
        ParseBasic(m_trafficData["train"],TRAIN);
        _INFO("train end");
    }

    if(m_trafficData.isMember("ship")){
        _INFO("ship");
        ParseBasic(m_trafficData["ship"],SHIP);
        _INFO("ship end");
    }


    if(m_trafficData.isMember("InterLine")){
        _INFO("InterLine");
        MJ::MyTimer t; t.start();
        ParseInterLine(m_trafficData["InterLine"]);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("InterLine end");
    }
    if (m_trafficData.isMember("roundTrip")){
        _INFO("roundTrip");
        MJ::MyTimer t; t.start();
        ParseRoundTrip(m_trafficData["roundTrip"]);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("roundTrip end");
    }

    if(m_trafficData.isMember("combine")){
        _INFO("combine");
        MJ::MyTimer t; t.start();
        ParseCombine(m_trafficData["combine"]);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("comnbine end");
    }
    if(m_trafficData.isMember("twiceCombine")){
        _INFO("twiceCombine");
        MJ::MyTimer t; t.start();
        ParseCombine(m_trafficData["twiceCombine"]);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("twiceCombine end");
    }

    GetCostEfficientScore(m_comparable_tickets_basic,m_cost_efficient_score_map_basic,m_debug_res);//获得性价比分数
    //以下设置三种基本数据类型的性价比得分
    if(m_trafficData.isMember("flight")){
        _INFO("set cost_efficient_score flight");
        MJ::MyTimer t; t.start();
        SetBasicCostEfficientScore(m_trafficData["flight"],FLIGHT);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("set flight end");
    }
    if (m_trafficData.isMember("bus")){
        _INFO("set cost_efficient_score bus");
        MJ::MyTimer t; t.start();
        SetBasicCostEfficientScore(m_trafficData["bus"],BUS);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("set bus end");
    }
    if (m_trafficData.isMember("train")){
        _INFO("set cost_efficient_score train");
        MJ::MyTimer t; t.start();
        SetBasicCostEfficientScore(m_trafficData["train"],TRAIN);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("set train end");
    }

    if (m_trafficData.isMember("ship")){
        _INFO("set cost_efficient_score ship");
        MJ::MyTimer t; t.start();
        SetBasicCostEfficientScore(m_trafficData["ship"],SHIP);
        _INFO("time cost %f",t.cost()*1.0/1000000);
        _INFO("set ship end");
    }
}


//设置三种基本数据类型的性价比得分
int Traffic::SetBasicCostEfficientScore(Json::Value &basic_data,TicketType type)
{
    vector<string> members_dept, members_dest;
    string dept_name, dest_name;
    members_dept = basic_data.getMemberNames();
    for (int i = 0; i < members_dept.size(); i++){
        dept_name = members_dept.at(i);
        Json::Value &dept = basic_data[dept_name];

        members_dest = dept.getMemberNames();
        for (int j = 0; j < members_dest.size(); j++){
            dest_name = members_dest.at(j);
            Json::Value &str_list = dept[dest_name];

            vector<vector<vector<string>>> tickets_list;
            int expected_index=-1;//同一个类别(GetCategory()),确定只有一张票为expected;
            for (int k = 0; k < str_list.size(); k++){
                if(!str_list[k].asString().empty())
                {
                    vector<vector<string>> ticket = ParseTicketStr(str_list[k].asString());
                    string cityPairDeptDate=GetCategory(ticket);
                    if(cityPairDeptDate.empty()) continue;
                    string unique_key=GetUniqueKey(ticket);
                    string cost_efficient_score_key=dept_name+"|"+dest_name+"|"+unique_key;
                    if(m_cost_efficient_score_map_basic.find(cost_efficient_score_key)!=m_cost_efficient_score_map_basic.end())
                    {
                        double adjust_score = m_cost_efficient_score_map_basic[cost_efficient_score_key];
                        double new_score = boost::lexical_cast<double>(ticket[0][2])+adjust_score;
                        if(m_expectedTickets.find(ticket[0][1])!=m_expectedTickets.end()) expected_index=tickets_list.size();
                        ticket[0][2]=boost::lexical_cast<string>(new_score);
                        str_list[k]=RecoveryTicketStr(ticket);
                        //m_debug_res[unique_key]["prefer_score"] =new_score;
                    }
                    double finalScore=boost::lexical_cast<double>(ticket[0][2]);
                    if(m_categorizedMaxScore[cityPairDeptDate]<finalScore) m_categorizedMaxScore[cityPairDeptDate]=finalScore;
                    tickets_list.push_back(ticket);
                }
            }
            if(expected_index>=0 and expected_index<tickets_list.size())
            {
                double old_score=boost::lexical_cast<double>(tickets_list[expected_index][0][2]);
                string cityPairDeptDate=GetCategory(tickets_list[expected_index]);
                //设置
                double new_score=old_score*1.2<m_categorizedMaxScore[cityPairDeptDate]?(m_categorizedMaxScore[cityPairDeptDate]+0.5):old_score*1.2;
                CheckScore(new_score);
                m_categorizedMaxScore[cityPairDeptDate]=new_score;
                tickets_list[expected_index][0][2]=boost::lexical_cast<string>(new_score);
            }
            FiltandSortTickets(tickets_list, str_list,type);
        }
    }
}


int Traffic::ParseBasic(Json::Value &basic_data,TicketType type)
{
    vector<string> trafficTool={"flight","train","bus","ship"};
    vector<string> members_dept, members_dest;
    string dept_name, dest_name;
    members_dept = basic_data.getMemberNames();
    for (int i = 0; i < members_dept.size(); i++){
        dept_name = members_dept.at(i);
        Json::Value &dept = basic_data[dept_name];

        members_dest = dept.getMemberNames();
        for (int j = 0; j < members_dest.size(); j++){
            dest_name = members_dest.at(j);
            _INFO("%s_%s",dept_name.c_str(),dest_name.c_str());
            MJ::MyTimer t; t.start();
            Json::Value &str_list = dept[dest_name];  //航班字符串列表

            m_depts.insert(dept_name);
            m_dests.insert(dest_name);

            m_price_adjust_tickets.clear();
            m_ticket_adjust_price.clear();
            if(m_price_no_adjust==false)
            {
                for (int k=0; k <str_list.size();k++){
                    if(!str_list[k].asString().empty()) m_price_adjust_tickets.push_back(str_list[k].asString());
                }
                //调整价格
                PriceAdjust(m_price_adjust_tickets);
            }

            int staticTicketCount=0,routineTicketCount=0;
            string deptDate,am_pm;
            for (int k = 0; k < str_list.size(); k++){
                if(!str_list[k].asString().empty())
                {
                    vector<vector<string>> ticket = GetScore(str_list[k].asString(), type, true,true);
                    if(ticket[0].size()>6 and boost::lexical_cast<int>(ticket[0][5])==1) staticTicketCount+=1; else routineTicketCount+=1;
                    if(deptDate=="")
                    {
                        deptDate = ticket[1][0].substr(0,8);
                        am_pm = ticket[1][0].substr(9,2);
                        if(am_pm<"14") am_pm="am"; else am_pm="pm";
                        if(IsCrossContinent(dept_name,dest_name)) am_pm="ap";
                    }
                    if(not Acceptable(str_list[k].asString()))
                    {
                        str_list[k]="";
                        continue;
                    }
                    string ticket_key=GetUniqueKey(ticket);
                    //设置价格
                    if(m_ticket_adjust_price.find(ticket_key)!=m_ticket_adjust_price.end())
                    {
                        double new_price=m_ticket_adjust_price[ticket_key];
                        double price_adjust=new_price-boost::lexical_cast<double>(ticket[0][0]);
                        ticket[0][0]=boost::lexical_cast<string>(new_price);
                        m_debug_res[ticket_key]["price_adjust"]=price_adjust;
                    }
                    str_list[k]=RecoveryTicketStr(ticket);

                    vector<string> tmp;
                    tmp.push_back(ticket[0][2]);//得分
                    tmp.push_back(ticket[0][0]);//价格
                    tmp.push_back(ticket_key);
                    m_comparable_tickets_basic[dept_name+"|"+dest_name].push_back(tmp);
                }
            }
            if(routineTicketCount>0 and staticTicketCount / routineTicketCount >= 5)
            {
                string exceptionInfo="from "+dept_name+" to "+dest_name+" at "+deptDate+" "+am_pm+" by "+trafficTool[type]+": staticCount "+boost::lexical_cast<string>(staticTicketCount)+" ,routineCount: "+boost::lexical_cast<string>(routineTicketCount);
                logException("ex21102",m_uid.c_str(),m_csuid.c_str(),m_qid.c_str(),exceptionInfo.c_str());
            }
            _INFO("time cost %f",t.cost()*1.0/1000000);
        }
    }
}

//根据城市对，获取两城市间的最短飞行时长  分钟
double Traffic::GetBestDur(const string& city1,const string& city2)
{
    int dis=0;
    bool suc=GetCityDistance(city1,city2,dis);
    if(suc==false)
    {
        return 5*60; //默认返回以最快飞行速度(250m/s)飞行5个小时;
    }
    return (dis*1.0/250)/60;
}

//针对基本数据类型
void Traffic::CheckCrossTimezone(string &ticket_str,TimezoneType & tzt)
{
    vector<string> dept_place,dest_place;
    vector<vector<string>> ticket = ParseTicketStr(ticket_str);
    int traffic_num=ticket.size()-1;
    boost::split(dept_place,ticket[1][2],boost::is_any_of("#"));
    boost::split(dest_place,ticket[traffic_num][3],boost::is_any_of("#"));
    if(dept_place.size()<2 or dest_place.size()<2 or dept_place[1].empty() or dest_place[1].empty())
    {
        ticket[0][2]=boost::lexical_cast<string>(0.0);//起止段没有归属城市为异常情况，将得分置为0
        ticket_str=RecoveryTicketStr(ticket);
        tzt=UNLEGAL;
        return;
    }
    else
    {
        string dept_city=dept_place[1];
        string dest_city=dest_place[1];
        if(m_city_country_map[dept_city]==m_city_country_map[dest_city] and (m_time_zone[dept_city]-m_time_zone[dest_city])<0.00001)
        {
            tzt=NOTCROSS;
            return;
        }
        else if(ticket_str.find("#|")!=string::npos)
        {
            ticket[0][2]=boost::lexical_cast<string>(0.0);//如果整张票首尾跨越时区，且中转的某个机场或车站又没有找到归属城市则报异常
            ticket_str=RecoveryTicketStr(ticket);
            tzt=UNLEGAL;
            return;
        }
        else
        {
            tzt=CROSS;
            return;
        }
    }
}

//处理: "去程航班1&回程航班1&&回程航班2"   的后半部分   ： 回程航班1&&回程航班2
int Traffic::ParseReturnFlights(string &str)
{
    vector<string> tickets_list;
    vector<string> tickets = StrSplit(str, "&&");
    for (int i = 0; i < tickets.size(); i++){
        if(!tickets[i].empty() and tickets[i].find("#|")==string::npos)
        {
            if(not Acceptable(tickets[i])) continue;
            vector<vector<string>> back_ticket = GetScore(tickets[i],FLIGHT);
            double total_score=boost::lexical_cast<double>(m_dept_ticket_multi[0][2])+boost::lexical_cast<double>(back_ticket[0][2]);
            vector<string> tmp;
            tmp.push_back(boost::lexical_cast<string>(total_score));
            tmp.push_back(back_ticket[0][0]); //回程票里的价格是往返总价
            tmp.push_back(GetUniqueKey(m_dept_ticket_multi)+"|"+GetUniqueKey(back_ticket));
            string dept_back_date=m_dept_ticket_multi[1][0].substr(0,8)+"|"+back_ticket[1][0].substr(0,8);//去程和回程的出发时间
            dept_back_date="all";
            m_comparable_tickets_multi[dept_back_date].push_back(tmp);
            tickets_list.push_back(RecoveryTicketStr(back_ticket));
        }
    }
    str = StrJoin(tickets_list, "&&");
}


// roundTrip,InterLine
// string 数组   "去程航班1&回程航班1&&回程航班2"
int Traffic::ParseMultiFlights(Json::Value &str_list,bool isInterLine)
{
    DateTime t1 = DateTime::Now();
    m_comparable_tickets_multi.clear();
    m_cost_efficient_score_map_multi.clear();
    for (int i = 0; i < str_list.size(); i++){
        vector<string> tickets;
        string oneArrayTickets = str_list[i].asString();
        std::size_t pos= oneArrayTickets.find('&');
        string dept_ticket_str=oneArrayTickets.substr(0,pos);
        //往返和联程票的出发日期必须和整个行程的起始日期相同
        if(m_startDate.size()==8 and dept_ticket_str.find(m_startDate)==string::npos)
        {
            str_list[i]="";
            continue;
        }
        if(not Acceptable(dept_ticket_str))
        {
            str_list[i]="";
            continue;
        }
        if(!dept_ticket_str.empty() and dept_ticket_str.find("#|")==string::npos)
        {
            tickets.push_back(dept_ticket_str);
            tickets.push_back(oneArrayTickets.substr(pos+1));
            //处理去程
            m_dept_ticket_multi=GetScore(tickets[0], FLIGHT);

            string cityPairDeptDate=GetCategory(m_dept_ticket_multi);
            double preferRatio=1.0;
            //倾向出联程方案:联程票整张票的性价比已经作用在返程票上,此处额外把去程票得分调高
            if(isInterLine) preferRatio = 1.3;
            double finalScore=boost::lexical_cast<double>(m_dept_ticket_multi[0][2])*preferRatio;
            CheckScore(finalScore);
            m_dept_ticket_multi[0][2]=lexical_cast<string>(finalScore);
            tickets[0] = RecoveryTicketStr(m_dept_ticket_multi);
            if(cityPairDeptDate!="" and m_categorizedMaxScore[cityPairDeptDate]<finalScore) m_categorizedMaxScore[cityPairDeptDate]=finalScore;
            //处理返程
            ParseReturnFlights(tickets[1]);
            //重组
            str_list[i] = StrJoin(tickets, "&");
        }
        else
        {
            //出发机场没有归属城市的情况基本不存在,而且此时数据是真的不能用
            str_list[i] = "";
        }
    }
    DateTime t2 = DateTime::Now();
    Json::Value tmp_res;
    GetCostEfficientScore(m_comparable_tickets_multi,m_cost_efficient_score_map_multi,tmp_res);//获得性价比分数/
    DateTime t3 = DateTime::Now();
    for (int i = 0; i < str_list.size(); i++){
        vector<string> tickets;
        string oneArrayTickets = str_list[i].asString();
        std::size_t pos= oneArrayTickets.find('&');
        string dept_ticket_str=oneArrayTickets.substr(0,pos);
        if(!dept_ticket_str.empty() and dept_ticket_str.find("#|")==string::npos)
        {
            tickets.push_back(dept_ticket_str);
            tickets.push_back(oneArrayTickets.substr(pos+1));
            //解析去程
            vector<vector<string>> dept_ticket_multi=ParseTicketStr(tickets[0]);
            //处理返程
            {
                vector<string> tickets_list;
                vector<string> back_tickets = StrSplit(tickets[1], "&&");
                for (int j = 0; j < back_tickets.size(); j++){
                    if(!back_tickets[j].empty() and back_tickets[j].find("#|")==string::npos)
                    {
                        vector<vector<string>> back_ticket = ParseTicketStr(back_tickets[j]);
                        string dept_back_date=dept_ticket_multi[1][0].substr(0,8)+"|"+back_ticket[1][0].substr(0,8);//去程和回程的出发时间
                        string unique_key=GetUniqueKey(dept_ticket_multi)+"|"+GetUniqueKey(back_ticket);
                        dept_back_date="all";
                        string key=dept_back_date+"|"+unique_key;
                        if(m_cost_efficient_score_map_multi.find(key)!=m_cost_efficient_score_map_multi.end())
                        {
                            double adjust_score = m_cost_efficient_score_map_multi[key];
                            double new_score = boost::lexical_cast<double>(back_ticket[0][2])+adjust_score;
                            string md5Con=dept_ticket_multi[0][1]+"|"+back_ticket[0][1];
                            //往返和联程票在行程两端,不影响城市分天,故可以直接置为满分
                            if(m_expectedTickets.find(md5Con)!=m_expectedTickets.end()) new_score=score_total;
                            CheckScore(new_score);
                            back_ticket[0][2]=boost::lexical_cast<string>(new_score);
                        }
                        string cityPairDeptDate=GetCategory(back_ticket);
                        double finalScore=boost::lexical_cast<double>(back_ticket[0][2]);
                        if(cityPairDeptDate!="" and m_categorizedMaxScore[cityPairDeptDate]<finalScore) m_categorizedMaxScore[cityPairDeptDate]=finalScore;
                        tickets_list.push_back(RecoveryTicketStr(back_ticket));
                    }
                }
                tickets[1] = StrJoin(tickets_list, "&&");
            }
            //重组
            str_list[i] = StrJoin(tickets, "&");
        }
    }
    DateTime t4 = DateTime::Now();
    _INFO("%f,%f,%f",(t2-t1).GetTotalSeconds(), (t3-t2).GetTotalSeconds(),(t4-t3).GetTotalSeconds());
}

//联程, 不用排序
int Traffic::ParseInterLine(Json::Value &interLine)
{
    vector<string> members = interLine.getMemberNames();
    vector<string> names1, names2;
    int interline_count = 0;
    for (int i = 0; i < members.size(); i++){
        string name = members.at(i);  //20003-10013:10008-20003

        vector<string> city_pairs = StrSplit(name, ":");
        m_depts.insert(StrSplit(city_pairs[0], "-")[0]);  //出发
        m_dests.insert(StrSplit(city_pairs[0], "-")[1]);  //到达
        m_depts.insert(StrSplit(city_pairs[1], "-")[0]);  //出发
        m_dests.insert(StrSplit(city_pairs[1], "-")[1]);  //到达

        Json::Value &str_list = interLine[name];  ////string 数组 .."去程航班1&回程航班1&&回程航班2"
        //返回联程票的数量
        for(int j = 0; j < str_list.size(); ++j)
        {
            string oneArrayTickets = str_list[j].asString();
            int ticket_count = count(oneArrayTickets.begin(),oneArrayTickets.end(),'&');
            interline_count += (ticket_count + 1) / 2;
        }
        ParseMultiFlights(str_list,true);
    }
    m_log_info.insert(pair<string,string>("interline_count",boost::lexical_cast<string>(interline_count)));
}

// 检查m_dest_citys中的城市是否都在列表中
bool Traffic::CheckLinkfull(tr1::unordered_set<string> names)
{
    if(m_dest_citys.size()<2) return false;
    for (tr1::unordered_set<string>::iterator it = m_dest_citys.begin(); it != m_dest_citys.end(); ++it){
        if (!names.count(*it)) return false;
    }
    return true;
}

int Traffic::ParseRoundTrip(Json::Value &flight)
{
    vector<string> members_dept, members_dest;
    vector<string> names;
    string dept_name, dest_name;
    members_dept = flight.getMemberNames();
    int round_count = 0;
    for (int i = 0; i < members_dept.size(); i++){
        dept_name = members_dept.at(i);
        Json::Value &dept = flight[dept_name];
        members_dest = dept.getMemberNames();
        for (int j = 0; j < members_dest.size(); j++){
            dest_name = members_dest.at(j);
            m_depts.insert(dept_name);
            m_dests.insert(dest_name);
            m_depts.insert(dest_name);
            m_dests.insert(dept_name);
            Json::Value &str_list = dept[dest_name];  //字符串列表  [去程航班1&回程航班1&&回程航班2, ....]
            //返回往返票的数量
            for(int i = 0; i < str_list.size(); ++i)
            {
                string oneArrayTickets = str_list[i].asString();
                int ticket_count = count(oneArrayTickets.begin(),oneArrayTickets.end(),'&');
                round_count += (ticket_count+1)/2;
            }
            ParseMultiFlights(str_list);
        }
    }
    m_log_info.insert(pair<string,string>("round_count",boost::lexical_cast<string>(round_count)));
}

//格式为:交通方式1_交通方式2_交通方式3|得分|过境签(1是过境签)|品质;;...
string Traffic::ParseCombineStr(const string &combine_str)
{
    vector<string> strs = StrSplit(combine_str, ";;");

    vector<string> summary = StrSplit(strs[0],"|");
    string types_str = summary[0];
    vector<string> types = StrSplit(types_str, "_");
    vector<TicketType> type_v;
    for (int i = 0; i < types.size(); i++){
        if (types[i] == "bus") type_v.push_back(BUS);
        else if (types[i] == "train") type_v.push_back(TRAIN);
        else type_v.push_back(FLIGHT);
    }

    vector<double> prefer_scores;
    vector<double> quality_scores;
    vector<int> dis_v;
    vector<string> citys;//只放每张票的首尾城市
    vector<string> times;
    vector<string> all_times;
    double total_price=0;
    string whole_key="";
    string dept_date="";
    for (int i = 1; i < strs.size(); i++){
        TicketType type = type_v[i-1];
        vector<vector<string>> t = GetScore(strs[i],type);
        string ticket_key=GetUniqueKey(t);
        //设置价格
        if(m_ticket_adjust_price.find(ticket_key)!=m_ticket_adjust_price.end())
        {
            t[0][0]=boost::lexical_cast<string>(m_ticket_adjust_price[ticket_key]);
        }

        prefer_scores.push_back(boost::lexical_cast<double>(t[0][2]));
        total_price+=boost::lexical_cast<double>(t[0][0]);
        if(i!=1) whole_key+="|";
        whole_key+=ticket_key;
        if(i==1) dept_date=t[1][0].substr(0,8);
        if(t[0].size()>=8) quality_scores.push_back(boost::lexical_cast<double>(t[0][7]));

        vector<string> dept_place,dest_place;
        boost::split(dept_place,t[1][2],boost::is_any_of("#"));
        boost::split(dest_place,t[t.size()-1][3],boost::is_any_of("#"));
        if(dept_place.size()==2 and dest_place.size()==2 and  not dept_place[1].empty() and not dest_place[1].empty())
        {
            string dept_city = dept_place[1];
            string dest_city = dest_place[1];
            citys.push_back(dept_city);
            citys.push_back(dest_city);
            times.push_back(t[1][0]);
            times.push_back(t[t.size()-1][1]);
            for(int j=1; j<t.size(); j++)
            {
                all_times.push_back(t[j][0]);
                all_times.push_back(t[j][1]);
            }
            // 根据城市取坐标， 再计算距离
            int dis;
            bool suc=GetCityDistance(dept_city,dest_city,dis);
            if (suc==false) dis=5*60*60*250;
            dis_v.push_back(dis);
            strs[i] = RecoveryTicketStr(t);
        }
        else //如果有一张票首或尾城市没找到，则整张票得分为0
        {
            summary[1]=boost::lexical_cast<string>(0.0);
            if(summary.size()>=4) summary[3]=boost::lexical_cast<string>(0.0);
            strs[0] = StrJoin(summary,"|");
            return StrJoin(strs, ";;");
        }
    }

    //根据距离比例计算，总分
    double prefer_score=0;
    for (int i = 0; i < prefer_scores.size(); i++){
        prefer_score += prefer_scores[i] * dis_v[i];
    }
    prefer_score =  prefer_score / accumulate(dis_v.begin(), dis_v.end(), 0.0);
    if(prefer_score>10) {_INFO("prefer_score booming:%f",prefer_score);}

    //拼中转一次打8折，拼两次打5折
    double combine_num_ratio=0.8;
    int combine_num = prefer_scores.size() - 1;
    if (combine_num == 2)
    {
        combine_num_ratio=0.5;
    }
    prefer_score*=combine_num_ratio;

    //得分调整:score=score*实际耗时/总耗时
    double stay_dur=0;
    for(int i =1;i<all_times.size()-1;i++)
    {
        if(i%2==1)
        {
            stay_dur+=CalDur(all_times[i], all_times[i+1]); //在一个城市换乘时,时区不会改变
        }
    }
    double time_cost = CalDur(times[0], times[times.size()-1], citys[0], citys[citys.size()-1]);
    double run_time_ratio=(time_cost-stay_dur)/time_cost;
    prefer_score*=run_time_ratio;

    //中转时间过短时,进行惩罚
    double trans_interval_ratio=1;
    for(int i=0;i<times.size()-1;i++)
    {
        if(i%2==1)
        {
            if(!CheckTrans(citys[i],times[i],type_v[i%2],citys[i+1],times[i+1],type_v[(i+1)%2]))
            {
                double trans_interval_ratio=0.05;
                break;
            }
        }
    }
    prefer_score *= trans_interval_ratio;

    double transit_visa_ratio=1;
    if(summary.size()>=3 and boost::lexical_cast<int>(summary[2])==1)
    {
        transit_visa_ratio=0.05;
    }
    prefer_score *=transit_visa_ratio;

    summary[1]=boost::lexical_cast<string>(prefer_score);

    vector<string> tmp;
    tmp.push_back(boost::lexical_cast<string>(prefer_score));
    tmp.push_back(boost::lexical_cast<string>(total_price));
    tmp.push_back(whole_key);
    dept_date="all";
    m_comparable_tickets_combine[dept_date].push_back(tmp);

    double quality_score=prefer_score;//由于异常导致品质得分无法计算出时，默认其得分和偏好得分一致
    if(quality_scores.size()==prefer_scores.size())
    {
        quality_score=0;
        for (int i = 0; i < quality_scores.size(); i++){
            quality_score += quality_scores[i] * dis_v[i];
        }
        quality_score =  quality_score / accumulate(dis_v.begin(), dis_v.end(), 0.0);
        if(quality_score>10) {_INFO("quality_score booming:%f",quality_score);}

        quality_score*=combine_num_ratio;
        quality_score*=run_time_ratio;
        quality_score *= trans_interval_ratio;
        quality_score *=transit_visa_ratio;
    }
    if(summary.size()>=4) summary[3]=boost::lexical_cast<string>(quality_score);

    //重组成字符串
    strs[0] = StrJoin(summary,"|");
    return StrJoin(strs, ";;");
}


int Traffic::ParseCombine(Json::Value &data)
{
    vector<string> members_dept, members_dest;
    string dept_name, dest_name;
    members_dept = data.getMemberNames();
    for (int i = 0; i < members_dept.size(); i++){
        dept_name = members_dept.at(i);
        Json::Value &dept = data[dept_name];
        members_dest = dept.getMemberNames();
        for (int j = 0; j < members_dest.size(); j++){
            dest_name = members_dest.at(j);
            Json::Value &str_list = dept[dest_name];  //字符串列表
            m_depts.insert(dept_name);
            m_dests.insert(dest_name);

            m_price_adjust_tickets.clear();
            m_ticket_adjust_price.clear();
            if(m_price_no_adjust==false)
            {
                for (int k = 0; k < str_list.size(); k++){
                    if(!str_list[k].asString().empty())
                    {
                        vector<string> strs = StrSplit(str_list[k].asString(), ";;");
                        for(int l=1;l<strs.size();l++) m_price_adjust_tickets.push_back(strs[l]);
                    }
                }
                PriceAdjust(m_price_adjust_tickets);
            }

            m_comparable_tickets_combine.clear();
            m_cost_efficient_score_map_combine.clear();
            for (int k = 0; k < str_list.size(); k++){
                if(!str_list[k].asString().empty())
                {
                    if(not Acceptable(str_list[k].asString()))
                    {
                        str_list[k]="";
                        continue;
                    }
                    str_list[k] = ParseCombineStr(str_list[k].asString());
                }
            }
            Json::Value tmp_res;
            GetCostEfficientScore(m_comparable_tickets_combine,m_cost_efficient_score_map_combine,tmp_res);//获得性价比分数
            int expected_index=-1; string category;
            for (int k = 0; k < str_list.size(); k++){
                if(!str_list[k].asString().empty())
                {
                    vector<string> strs = StrSplit(str_list[k].asString(), ";;");
                    string dept_date="";
                    string whole_key="";
                    string md5Con="";
                    string am_pm="";
                    for(int l=1;l<strs.size();l++)
                    {
                        vector<vector<string>> t=ParseTicketStr(strs[l]);
                        if(l==1)
                        {
                            dept_date=t[1][0].substr(0,8);
                            am_pm = t[1][0].substr(9,2);
                            if(am_pm<"14") am_pm="am"; else am_pm="pm";
                            if(IsCrossContinent(dept_name,dest_name)) am_pm="ap";
                        }
                        if(l!=1)
                        {
                            whole_key+="|";
                            md5Con+="|";
                        }
                        whole_key+=GetUniqueKey(t);
                        md5Con+=t[0][1];
                    }
                    string key="all|"+whole_key;
                    vector<string> summary=StrSplit(strs[0],"|");
                    string cityPairDeptDate=dept_name+"_"+dest_name+"_"+dept_date+"_"+am_pm;
                    if(m_cost_efficient_score_map_combine.find(key)!=m_cost_efficient_score_map_combine.end())
                    {
                        double adjust_score=m_cost_efficient_score_map_combine[key];
                        double new_score=boost::lexical_cast<double>(summary[1])+adjust_score;
                        if(m_expectedTickets.find(md5Con)!=m_expectedTickets.end())
                        {
                            expected_index=k;
                            category=cityPairDeptDate;
                        }
                        summary[1]=boost::lexical_cast<string>(new_score);
                        strs[0] = StrJoin(summary,"|");
                        str_list[k]=StrJoin(strs, ";;");
                    }
                    double finalScore=boost::lexical_cast<double>(summary[1]);
                    if(m_categorizedMaxScore[cityPairDeptDate]<finalScore) m_categorizedMaxScore[cityPairDeptDate]=finalScore;
                }
            }
            if( expected_index>=0 and expected_index<str_list.size())
            {
                vector<string> strs = StrSplit(str_list[expected_index].asString(), ";;");
                vector<string> summary=StrSplit(strs[0],"|");
                double old_score=boost::lexical_cast<double>(summary[1]);
                double new_score=old_score*1.2<m_categorizedMaxScore[category]?(m_categorizedMaxScore[category]+0.5):old_score*1.2;
                CheckScore(new_score);
                m_categorizedMaxScore[category]=new_score;
                summary[1]=boost::lexical_cast<string>(new_score);
                strs[0] = StrJoin(summary,"|");
                str_list[expected_index]=StrJoin(strs, ";;");
            }
        }
    }
}


bool ScoreComp(const vector<vector<string>>& f1, const vector<vector<string>>& f2)
{
    return boost::lexical_cast<double>(f1[0][2]) > boost::lexical_cast<double>(f2[0][2]);
}

bool PriceComp(const vector<vector<string>>& f1, const vector<vector<string>>& f2)
{
    return boost::lexical_cast<double>(f1[0][0]) < boost::lexical_cast<double>(f2[0][0]);
}

int Traffic::FiltandSortTickets(vector<vector<vector<string>>>& tickets_list, Json::Value &res,TicketType type)
{
    vector<vector<vector<string>>> hour_best;
    if(type==FLIGHT or type==TRAIN or type==BUS or type == SHIP)
    {
        tr1::unordered_map<string,vector<vector<vector<string>>>> similar_tickets;
        for (int i = 0; i < tickets_list.size(); i++){
            string dept_hour=tickets_list[i][1][0].substr(0,11);
            similar_tickets[dept_hour].push_back(tickets_list[i]);

            //string dest_hour=tickets_list[i][tickets_list[i].size()-1][1].substr(0,11);
            //string dept_dest_hour=dept_hour+"|"+dest_hour;
            //similar_tickets[dept_dest_hour].push_back(tickets_list[i]);
        }
        for(tr1::unordered_map<string,vector<vector<vector<string> > > >::iterator iter=similar_tickets.begin();iter!=similar_tickets.end();iter++)
        {
            /*if(iter->second.size()>1)
              {
              _INFO("dept_dest_time:%s",iter->first.c_str());
              }
              double last_price =0;*/

            sort(iter->second.begin(),iter->second.end(),PriceComp);
            double max_score =0;
            for(int i=0; i< iter->second.size(); i++)
            {
                double cur_score=boost::lexical_cast<double>(iter->second[i][0][2]);
                /*_INFO("%f ",cur_score);
                  double cur_price=boost::lexical_cast<double>(iter->second[i][0][0]);
                  _INFO("last_price:%f",last_price);
                  _INFO("max_score:%f",max_score);
                  _INFO("cur_price:%f",cur_price);
                  _INFO("cur_score:%f",cur_score);
                  last_price=cur_price;*/
                if(cur_score>max_score)
                {
                    /*_INFO("left");
                      last_price=cur_price;*/
                    hour_best.push_back(iter->second[i]);
                    max_score=cur_score;
                }
                else
                {
                    string unique_key=GetUniqueKey(iter->second[i]);
                    m_debug_res.removeMember(unique_key);
                    //_INFO("filt");
                }
            }
        }
    }
    res.clear();
    sort(hour_best.begin(), hour_best.end(), ScoreComp);
    for (int i = 0; i < hour_best.size(); i++){
        res.append(RecoveryTicketStr(hour_best[i]));
    }
}

//飞机转飞机至少1个小时，大巴转大巴至少15分钟，火车转火车在日本国内至少10分钟其它30分钟,换交通方式至少4个小时
bool Traffic::CheckTrans(const string& city1, const string& time1,TicketType type1, const string& city2, const string& time2,TicketType type2)
{
    int trans_interval=CalDur(time1,time2);
    if(type1==FLIGHT and type2==FLIGHT)
    {
        return trans_interval >= 60;
    }
    else if(type1==BUS and type2==BUS)
    {
        return trans_interval >= 15;
    }
    else if(type1==TRAIN and type2==TRAIN)
    {
        if(m_city_country_map[city1]=="日本" and m_city_country_map[city2]=="日本")
        {
            return trans_interval >= 10;
        }
        else
        {
            return trans_interval >= 30;
        }
    }
    else if(type1 == SHIP and type2 == SHIP)
    {
        return trans_interval >=30;
    }
    else
    {
        return trans_interval >= 60*4;
    }
}

//获得两个城市之间的距离
bool Traffic::GetCityDistance(const string& city1,const string& city2,int &dis)
{
    // 根据城市取坐标,再计算距离
    vector<string> mapinfo1;
    vector<string> mapinfo2;
    if (m_mapinfo.count(city1)==0 or m_mapinfo.count(city2)==0)
    {
        return false;
    }

    boost::split(mapinfo1, m_mapinfo[city1],boost::is_any_of(", "),boost::token_compress_on);
    boost::split(mapinfo2, m_mapinfo[city2],boost::is_any_of(", "),boost::token_compress_on);

    if (mapinfo1.size()!=2 or mapinfo2.size()!=2)
    {
        return false;
    }

    double lat1 = boost::lexical_cast<double>(mapinfo1[1]);
    double lng1 = boost::lexical_cast<double>(mapinfo1[0]);
    double lat2 = boost::lexical_cast<double>(mapinfo2[1]);
    double lng2 = boost::lexical_cast<double>(mapinfo2[0]);
    dis=GetDistance(lat1, lng1, lat2, lng2);
    return true;
}

//判断两个城市是否属于区域内
bool Traffic::IsIntraArea(const string& city1, const string& city2)
{
    // 根据城市取坐标， 再计算距离
    int dis=0;
    bool support=GetCityDistance(city1,city2,dis);
    return (support and dis < 2000*1000 && m_city_continent.count(city1) && m_city_continent.count(city2) &&
            m_city_continent[city1] == m_city_continent[city2]);
}

bool Traffic::IsCrossContinent (const string& city1, const string& city2)
{
    return m_city_continent.count(city1) && m_city_continent.count(city2) && m_city_continent[city1] != m_city_continent[city2];
}

//判断两个城市间交通是否是短途
bool Traffic::IsShortTraffic(const string& dept, const string& dest)
{
    // 根据城市取坐标， 再计算距离
    int dis=0;
    bool support=GetCityDistance(dept, dest, dis);
    return support and dis <= 1000*1000;
}

int Traffic::BadTime(const string& timestr, bool is_dept,bool isInstraArea,TicketType type)
{
    if(type==FLIGHT)
    {
        return FlightBadTime(timestr,is_dept,isInstraArea);
    }
    else if(type==TRAIN or type==BUS or type == SHIP)
    {
        return TrainBadTime(timestr,is_dept);
    }
    else
    {
        assert(false);
    }
}

int Traffic::FlightBadTime(const string& timestr, bool isdept, bool isIntraArea)
{
    DateTime t = DateTime::Parse("20000101_" + timestr, "yyyyMMdd_HH:mm");
    if (!isIntraArea){
        if (isdept){
            if (m_24h[11] <= t && t <= m_24h[20]) return GOOD; //good
            if (m_24h[1] <= t && t <= m_24h[8]) return BAD; //bad
            return ACCEPT;
        }
        else{
            if (m_24h[8] <= t && t <= m_24h[18]) return GOOD;
            if (m_24h[18] <= t && t <= m_24h[23] || m_24h[5] <= t && t <= m_24h[8]) return ACCEPT;
            return BAD;
        }
    }
    else{
        if (isdept){
            if (m_24h[10] <= t && t <= m_24h[18]) return GOOD;
            if (m_24h[18] <= t && t <= m_24h[21] || m_24h[8] <= t && t <= m_24h[10]) return ACCEPT;
        }
        else{
            if (m_24h[9] <= t && t <= m_24h[19]) return GOOD;
            if (m_24h[19] <= t && t <= m_24h[22] || m_24h[6] <= t && t <= m_24h[9]) return ACCEPT;
        }
        return BAD;

    }
}

//火车大巴的出发到达时间的好坏分级
int Traffic::TrainBadTime(const string& timestr, bool isdept)
{
    DateTime t = DateTime::Parse("20000101_" + timestr, "yyyyMMdd_HH:mm");
    if (isdept){
        if (m_24h[9] <= t && t <= m_24h[19]) return GOOD;
        if (m_24h[19] <= t && t <= m_24h[22] || m_24h[8] <= t && t <= m_24h[9]) return ACCEPT;
    }
    else{
        if (m_24h[8] <= t && t <= m_24h[19]) return GOOD;
        if (m_24h[19] <= t && t <= m_24h[22] || m_24h[6] <= t && t <= m_24h[8]) return ACCEPT;
    }
    return BAD;
}

// 考虑时区
double Traffic::CalDur(const string& timestr1, const string& timestr2, const string& city1, const string& city2)
{
    DateTime time1, time2;
    time1 = DateTime::Parse(timestr1, "yyyyMMdd_HH:mm");
    time2 = DateTime::Parse(timestr2, "yyyyMMdd_HH:mm");

    //夏令时
    float time_diff1 = m_time_zone[city1];
    float time_diff2 = m_time_zone[city2];
    if (m_summer.count(city1) == 1){
        vector<DateTime> summer1 = m_summer[city1];
        if (summer1[0] < time1 && time1 < summer1[1] || (summer1[2] < time1 && time1 < summer1[3])){
            time_diff1 = m_summer_zone[city1];
        }
    }
    if (m_summer.count(city2) == 1){
        vector<DateTime> summer2 = m_summer[city2];
        if (summer2[0] < time2 && time2 < summer2[1] || (summer2[2] < time2 && time2 < summer2[3])){
            time_diff2 = m_summer_zone[city2];
        }
    }

    int t1 = (int) (time_diff1 * 60);
    int t2 = (int) (time_diff2 * 60);

    TimeSpan durtime = time2 - TimeSpan(t2 / 60, t2 % 60, 0) - (time1 - TimeSpan(t1 / 60, t2 % 60, 0));
    double dur =  durtime.GetTotalMinutes();
    return dur;
}

// 计算两个时间(字符串)的差值，精确到分。  时间格式： 20160816_00:10
double Traffic::CalDur(const string& timestr1, const string& timestr2)
{
    DateTime time1, time2;
    time1 = DateTime::Parse(timestr1, "yyyyMMdd_HH:mm");
    time2 = DateTime::Parse(timestr2, "yyyyMMdd_HH:mm");
    TimeSpan durtime = time2 - time1;
    double dur =  durtime.GetTotalMinutes();
    return dur;
}

int Traffic::SetQualityM(const Json::Value& debug)
{
    cout << "SetQualityM" << endl;
    if (debug.empty()) return -1;
    if (debug.isMember("proportion") && !debug["proportion"].empty()){
        m_proportionV.clear();
        Json::Value tmp = debug["proportion"];
        for (int i = 0; i < tmp.size(); i++)
            m_proportionV.push_back(tmp[i].asDouble());
    }

    cout << endl;
    //cout_matrix(m_proportionV);

    if (debug.isMember("quality") && !debug["quality"].empty()){
        m_qualityM.clear();
        for (int i = 0; i < debug["quality"].size(); i++){
            Json::Value tmp  = debug["quality"][i];
            vector<double> qualityV;
            for (int j = 0; j < tmp.size(); j++){
                qualityV.push_back(tmp[j].asDouble());
            }
            m_qualityM.push_back(qualityV);
        }
    }
    cout << endl;
    //cout_matrix(m_qualityM);
}


int Traffic::SetDefaultQualityM()
{
    // ----------------flight -------------------
    // 舱位，中转，耗时，起飞时间，降落时间，航空公司，中转转移(换一次扣1/2)，中转换航空公司(换一次扣1/2)
    double flight_V[] = {12, 8, 15, 2, 2, 3, 6, 4}; //舱位和中转主要靠偏好来惩罚
    m_proportionV.assign(flight_V, flight_V + 8);
    //品评矩阵
    /*飞机舱位,当id为0时，表示舱位未知，此时默认其为经济舱
      +----+-----------------+
      | id | seat_type       |
      +----+-----------------+
      | 0 |            |
      | 1 | 经济舱     |
      | 2 | 超级经济舱 |
      | 3 | 商务舱     |
      | 4 | 头等舱     |
      +----+-----------------+
      */
    /*火车舱位,当id为0时，表示舱位未知，此时认为其为无座，及id 为0时和id为5时表达的意义相同,为计算统一，id对5取模作为真正的id; 大巴的舱位直接对应到无座
      +----+--------------+
      | id | seat_type    |
      +----+--------------+
      | 0 |          |
      | 1 | 标准座   |
      | 2 | 商务座   |
      | 3 | 普通卧铺 |
      | 4 | 高级卧铺 |
      | 5 | 无座     |
      +----+--------------+
      */

    double f_cabin[] = {20, 15, 12, 11, 8};
    //中转属性 之所默认差距很大，是因为一般中转次数越多，所耗时间越长;要区分中转两次和更多
    double stop[] = {10, 5, 1, 0};
    double dept_time[] = {4, 3, 1};
    double dest_time[] = {4, 3, 1};
    double airline[] = {10, 7, 5, 2, 1};

    vector<double> qualityV0(f_cabin, f_cabin + 5);
    vector<double> qualityV1(stop, stop + 4);
    vector<double> qualityV2(1, 1);
    vector<double> qualityV3(dept_time, dept_time+3);
    vector<double> qualityV4(dest_time, dest_time+3);
    vector<double> qualityV5(airline, airline + 5);
    vector<double> qualityV6(1, 1), qualityV7(1, 1);
    m_qualityM.clear();
    m_qualityM.push_back(qualityV0);
    m_qualityM.push_back(qualityV1);
    m_qualityM.push_back(qualityV2);
    m_qualityM.push_back(qualityV3);
    m_qualityM.push_back(qualityV4);
    m_qualityM.push_back(qualityV5);
    m_qualityM.push_back(qualityV6);
    m_qualityM.push_back(qualityV7);
}


//设置默认交通偏好
void Traffic::SetDefaultPreference()
{
    m_accept_train=true;//能接受火车
    m_accept_bus=true;//接受大巴
    m_accept_ship = true;//接受轮渡
    m_accept_flight_stop=1;//飞机最多一次中转
    m_accept_dept_early=true;//接受早起
    m_accept_dest_late=true;//接受晚到
    m_prefer_union.clear();//空表示无偏好

}

// 获取交通偏好
/*
prefer:json对象 存储用户的偏好选择
#行程参数：
global: json对象
mode: string数组 "flight，train，bus，driving，ship" 复选，后台默认“flight,train”
transit :int  表示允许的中转次数 (1:直达 2:中转一次 3:中转2次及以上） 后台默认1
time: int数组 1/2/3  交通时间要求 复选  (1:不要起飞时间早的航班（0:00-6:00） 2:不要夜间飞行航班（0:00-6:00）3:不要到达时间晚的航班（22:00-6:00） )  后台默认空数组表示不限
#飞机参数：
flight:
com : string数组  表示用户选定的航空公司ID 参考航空公司ID名称对应关系  后台默认空数组 表示不限
type : int数组   机型偏好 客户端单选 后台复选 (1:小型飞机 2:中型飞机 3:大型飞机) 默认空数组表示没有要求
class : int数组  飞机的舱位 客户端单选 后台复选 （1:经济舱 2:超级经济舱 3:商务舱 4:头等舱 5：廉航套餐） 后台默认[]表示不限
#火车参数：
train:
class : int数组   火车的座位席别 复选  (1:二等舱 2:商务舱 3:一等舱 4:卧铺 后台默认[]表示不限)
#租车参数：
driving（暂时不用）:
enable:int 0/1 是否租车 1为租车 默认0
prefer: int 0:全程租车 1:智能分段租车  默认0
gear: int 汽车档位 0:不限 1:手动挡 2:自动挡)
*/
int Traffic::GetPreferencesM(Json::Value &prefer)
{
    m_price_prefer_ratio=price_prefer_ratio_comfortable;
    if (prefer.isMember("global")){
        if (prefer["global"].isMember("mode"))
        {
            Json::Value &mode = prefer["global"]["mode"];
            bool train_selected = false;
            for (int i = 0; i < mode.size(); ++i){
                string tmp = mode[i].asString();
                if (tmp == "train")
                {
                    train_selected=true;
                    break;
                }
            }
            if (train_selected == false) m_accept_train = false;

            bool ship_selected = false;
            for(int i = 0; i < mode.size(); ++i){
                string tmp = mode[i].asString();
                if(tmp == "ship")
                {
                    ship_selected = true;
                    break;
                }
            }
            if(ship_selected == false) m_accept_ship = false;
            for (int i = 0; i < mode.size(); ++i){
                string tmp = mode[i].asString();
                if (tmp == "bus")
                {
                    m_accept_bus = true;
                }
            }
        }

        if (prefer["global"].isMember("transit")){
            int transit=prefer["global"]["transit"].asInt();
            if( transit>=1 and transit<=3)
            {
                m_accept_flight_stop=transit-1;
            }
        }

        if (prefer["global"].isMember("time"))
        {
            Json::Value &time = prefer["global"]["time"];
            for (int i = 0; i < time.size(); ++i){
                if(time[i].asInt()==1)
                {
                    m_accept_dept_early=false;
                }
                else if(time[i].asInt()==3)
                {
                    m_accept_dest_late=false;
                }
            }
        }
    }

    if (prefer.isMember("flight"))
    {
        if (prefer["flight"].isMember("com"))
        {
            Json::Value & com=prefer["flight"]["com"];
            for (int i =0; i<com.size(); ++i)
            {
                if(com[i]=="u101")
                {
                    m_prefer_union.insert("天合联盟");
                }
                else if(com[i]=="u103")
                {
                    m_prefer_union.insert("星空联盟");
                }
                else if(com[i]=="u102")
                {
                    m_prefer_union.insert("寰宇一家");
                }
            }
        }

        if (prefer["flight"].isMember("class")){
            Json::Value classes = prefer["flight"]["class"];
            for (int i = 0; i < classes.size(); i++){
                int prefer_class = classes[i].asInt();
                if(prefer_class==5) prefer_class=1;//联航套餐被视为经济舱
                if (prefer_class==1 or prefer_class==3 or prefer_class==4 ) m_accept_cabin.insert(prefer_class);
                if(prefer_class==3 or prefer_class==4)
                {
                    m_price_prefer_ratio=price_prefer_ratio_luxury;
                    //当为奢侈型偏好时,则推测其偏好直达且不接受早起和晚到
                    m_accept_flight_stop=0;//只要用户选中了头等舱或商务舱(无论是单选还是复选),就认为其偏好直达
                    m_accept_dept_early=false;
                    m_accept_dest_late=false;
                }
            }
            if(m_accept_cabin.empty()) m_accept_cabin.insert(1);

            //如果用能接受头等舱和经济舱,则认为其也能接受商务舱
            if(*m_accept_cabin.begin()==1 and *m_accept_cabin.rbegin()==4) m_accept_cabin.insert(3);

            if(classes.size()==1 and classes[0u].asInt()==1)
            {
                if(prefer.isMember("hotel") && prefer["hotel"].isMember("type")){
                    Json::Value type_prefer = prefer["hotel"]["type"];
                    for (int i = 0; i < type_prefer.size(); i++){
                        if (type_prefer[i].asInt() == 5){
                            m_price_prefer_ratio=price_prefer_ratio_lightLuxury;
                            break;
                        }
                    }
                    if(type_prefer.size()==1 and type_prefer[0].asInt()==13) m_price_prefer_ratio=price_prefer_ratio_economic;
                }
            }
        }
    }
    else
    {
        m_accept_cabin.insert(1);
    }

    if(prefer.isMember("global"))
    {
        double price_prefer_ratio=prefer["global"].get("price_prefer_ratio",-1.0).asDouble();
        if(price_prefer_ratio-0>eps and price_prefer_ratio-1.0<-eps)
        {
            m_price_prefer_ratio=price_prefer_ratio;
        }
    }
}


// 根据票的信息确定属性矩阵和偏好系数
// //flight: 价格|MD5|后台交通得分|来源数目|来源|是否静态数据(1是静态数据)|过境签(1是过境签)|品质（目前均赋为0）;出发时间|到达时间|出发机场|目的机场|航空公司ID|仓位ID|机型ID;出发时间|到达时间|出发机场|目的机场|航空公司ID|仓位ID|机型ID
// //train: 价格|MD5|后台交通得分|来源数目|来源|是否静态数据(1是静态数据)|过境签(1是过境签)|品质;出发时间|到达时间|出发机场|目的机场|仓位ID;出发时间|到达时间|出发机场|目的机场|仓位ID
// //bus: 价格|MD5|后台交通得分|来源数目|来源|是否静态数据(1是静态数据)|过境签(1是过境签)|品质;出发时间|到达时间|出发机场|目的机场;出发时间|到达时间|出发机场|目的机场
vector<vector<double>> Traffic::GetPropertyM(const vector<vector<string>> &tickets,TicketType type,double &prefer_ratio,double & quality_ratio, Json::Value &cur_ticket,TimezoneType status,bool isBasic)
{
    cur_ticket["prefer_ratios"] = Json::Value();
    cur_ticket["quality_ratios"] = Json::Value();
    quality_ratio=1;
    int traffic_num = tickets.size()-1;

    //耗时计算,CheckCrossTimezone保证首尾必有归属城市
    string start = StrSplit(tickets[1][2], "#")[1];
    string end = StrSplit(tickets[traffic_num][3], "#")[1];
    cur_ticket["dept"] = start;
    cur_ticket["dest"] = end;

    bool is_intra= true;
    is_intra = IsIntraArea(start, end);

    double best_dur = GetBestDur(start,end);
    double time_cost = CalDur(tickets[1][0], tickets[traffic_num][1], start, end);
    cur_ticket["time_cost"] = time_cost;
    int extra_time=0;
    if(type==FLIGHT) extra_time=60*2; //飞机额外增加往返机场和候机时间共计2个小时
    double dur_times = (time_cost+60*12+extra_time) / (best_dur+60*12);
    double dur_score = 0;
    if (dur_times > 3 ){
        dur_score = 0;
    }
    else if (dur_times > 2 ){
        dur_score = -0.2 * dur_times + 0.6;
    }
    else if ( dur_times > 1){
        dur_score = -0.8 * dur_times + 1.8;
    }
    else{
        dur_score = 1;
    }
    vector<double> V_dur(1, dur_score);


    //中转转移:   y=1-x/2 (x为中转转移的次数)
    //换航空公司: y=1-x/2 (x为换航空公司的次数)
    int stop_transit_count = 0;
    int airline_change_count = 0;
    for (int i = 0; i < traffic_num-1; ++i){
        if (tickets[i+1][3] != tickets[i+2][2]) stop_transit_count += 1;
        if (type == FLIGHT and tickets[i+1][4] != tickets[i+2][4]) airline_change_count += 1;
    }
    double stop_transit = 1 - stop_transit_count / 2.0;
    double airline_change = 1 - airline_change_count / 2.0;
    stop_transit = stop_transit < 0 ? 0 : stop_transit;
    airline_change = airline_change < 0 ? 0 : airline_change;
    vector<double> V_stop_transit(1, stop_transit);
    vector<double> V_airline_change(1, airline_change);


    //下面属性是需要计算子段航班
    vector<int> cabins;
    vector<int> dept_times;
    vector<int> dest_times;
    vector<int> airline_stars;
    vector<int> unions;
    vector<string>dept_dest_times; //用于判断中转时间是否合理
    vector<string>citys;
    for (int i = 1; i <= traffic_num; i++){
        vector<string> traffic = tickets[i];
        int cabin_index=4;//火车数据舱位的下标
        if(type==FLIGHT or type == SHIP) cabin_index=5;
        int cabin = 0;//大巴默认对应到 无座 ,因为一般大巴远没火车和飞机舒服,特别是美国大巴
        if (type!=BUS and !traffic.at(cabin_index).empty()) {
            int _cabin = boost::lexical_cast<int>(traffic.at(cabin_index));
            if(_cabin==0 and type==FLIGHT) _cabin=1;//飞机默认是经济舱
            if(type == SHIP)
            {
                if(_cabin == 2)
                    _cabin = 4;
            }
            if(_cabin<=5 and _cabin>=0) cabin=_cabin%5;
        }
        cabins.push_back(4-cabin);

        if(status==CROSS)
        {
            vector<string> dept_place,dest_place;
            boost::split(dept_place,traffic.at(2),boost::is_any_of("#"));
            boost::split(dest_place,traffic.at(3),boost::is_any_of("#"));
            //CheckCrossTimezone保证首尾跨越时区时，进来票的中转机场或车站必有归属城市
            if(dept_place.size()==2 and dest_place.size()==2 and  not dept_place[1].empty() and not dest_place[1].empty())
            {
                string dept_city = dept_place[1];
                string dest_city = dest_place[1];
                citys.push_back(dept_city);
                citys.push_back(dest_city);
            }
        }

        if(i==1)
        {
            string dept_time = traffic.at(0).substr(9, 5);
            bool is_dept=true;
            int time_type = BadTime(dept_time, is_dept, is_intra,type);
            dept_times.push_back(time_type);
        }
        if(i==traffic_num)
        {
            string dest_time = traffic.at(1).substr(9, 5);
            bool is_dept=false;
            int time_type = BadTime(dest_time, is_dept, is_intra,type);
            dest_times.push_back(time_type);
        }
        dept_dest_times.push_back(traffic[0]);
        dept_dest_times.push_back(traffic[1]);

        if(type==FLIGHT)
        {
            //航空公司等级  5级， 1-5， 越小越好
            string airline = traffic.at(4);
            int star = boost::lexical_cast<int>(m_airlines.count(airline) ? m_airlines[airline][0] : "4");
            airline_stars.push_back(star-1);//放入1-5之外的数据时,在归一化时会被自动地忽略

            //是否命中联盟,unions中存的是下标,存入下标0代表符合联盟选择
            string union_s = m_airlines.count(airline) ? m_airlines[airline][1] : "";
            //_INFO("airline:%s,union:%s",airline.c_str(),union_s.c_str());
            if(m_prefer_union.empty() or m_prefer_union.find(union_s)!=m_prefer_union.end() )
            {
                unions.push_back(0);
            }
            else
            {
                unions.push_back(1);
            }
        }
        else
        {
            airline_stars.push_back(0);////火车运营公司登记默认对应优质航空航空
            unions.push_back(0);
        }
    }


    //舱位和中转联合对比
    //偏好头等舱+直达，机票推荐优先级：
    //城间距离>1000km：头等舱+直达>头等舱+中转一次>商务舱+直达>商务舱+中转一次>经济舱+直达>头等舱+中转两次>商务舱+中转两次>经济舱+中转一次>经济舱+中转两次
    //
    //城间距离<1000km：头等舱+直达>商务舱+直达>经济舱+直达>头等舱+中转一次>商务舱+中转一次>经济舱+中转一次
    //
    //
    //偏好商务舱+直达，机票推荐优先级：
    //城间距离>1000km：商务舱+直达>商务舱+中转一次>经济舱+直达>商务舱+中转两次>经济舱+中转一次>经济舱+中转两次
    //城间距离<1000km：商务舱+直达>经济舱+直达>商务舱+中转一次>经济舱+中转一次
    //
    //
    //偏好头等舱+商务舱+直达，机票推荐优先级：
    //城间距离>1000km：头等舱+直达>商务舱+直达>头等舱+中转一次>商务舱+中转一次>头等舱+中转两次>商务舱+中转两次>经济舱+直达>经济舱+中转一次>经济舱+中转两次
    //
    //城间距离<1000km：头等舱+直达>商务舱+直达>经济舱+直达>头等舱+中转一次>商务舱+中转一次>经济舱+中转一次
    //
    //偏好商务舱+经济舱+直达，机票推荐优先级：
    //城间距离>1000km：商务舱+直达>经济舱+直达>商务舱+中转一次>经济舱+中转一次>商务舱+中转两次>经济舱+中转两次
    //
    //
    //城间距离<1000km：商务舱直达>经济舱+直达>商务舱+中转一次>经济舱+中转一次

    int stop = traffic_num -1;
    stop = (stop > 2) ? 3: stop;
    vector<double> V_stop(4, 0);
    V_stop[stop] = 1;
    double cabin_stop_ratio=1;
    if (type==FLIGHT)
    {
        //舱位判定,按最高舱位来判定其整体的舱位,但是不同舱位混拼会有额外降权
        sort(cabins.begin(),cabins.end());
        int final_cabin=4-cabins.front();

        double multi_cabin_ratio=1;
        if (cabins.front()!=cabins.back()) multi_cabin_ratio=0.8;
        prefer_ratio*=multi_cabin_ratio;
        cur_ticket["prefer_ratios"]["multi_cabin_ratio"] = multi_cabin_ratio;
        if(m_accept_cabin.size()==1)
        {
            if(*m_accept_cabin.begin()==1)//只接受经济舱
            {
                stop-=m_accept_flight_stop;//特例:当只接受经济舱时,stop的含义改为stop_diff
                if(stop<0) stop=0;
                if (not IsShortTraffic(start,end))
                {
                    map<int,double> ratios;
                    ratios[10]=1.0;
                    ratios[11]=0.5;
                    ratios[12]=0.2;
                    ratios[13]=1.0/10;
                    ratios[30]=0.5/10;
                    ratios[31]=0.2/10;
                    ratios[32]=1.0/100;
                    ratios[33]=0.5/100;
                    ratios[40]=0.2/100;
                    ratios[41]=1.0/1000;
                    ratios[42]=0.5/1000;
                    ratios[43]=0.2/1000;

                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
                else
                {
                    map<int,double> ratios;
                    ratios[10]=1.0;
                    ratios[11]=0.5;
                    ratios[12]=0.2;
                    ratios[30]=1.0/10;
                    ratios[31]=0.5/10;
                    ratios[32]=0.2/10;
                    ratios[40]=1.0/100;
                    ratios[41]=0.5/100;
                    ratios[42]=0.2/100;

                    if(stop>2) stop=2;
                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }

            }
            else if(*m_accept_cabin.begin()==3)//只接受商务舱
            {
                if (not IsShortTraffic(start,end))
                {
                    //舱位中转比率表，key中的十位表示舱位，个位表示中转次数，value表示对应的比率
                    map<int,double> ratios;
                    ratios[30]=1.0;
                    ratios[31]=0.5;
                    ratios[10]=0.2;
                    ratios[32]=1.0/10;
                    ratios[11]=0.5/10;
                    ratios[12]=0.2/10;
                    ratios[33]=1.0/100;
                    ratios[13]=0.5/100;
                    ratios[40]=0.2/100;
                    ratios[41]=1.0/1000;
                    ratios[42]=0.5/1000;
                    ratios[43]=0.2/1000;

                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
                else
                {
                    map<int,double> ratios;
                    ratios[30]=1.0;
                    ratios[10]=0.8;
                    ratios[31]=1.0/10;
                    ratios[11]=0.8/10;
                    ratios[32]=1.0/100;
                    ratios[12]=0.8/100;
                    ratios[40]=1.0/1000;
                    ratios[41]=0.5/1000;
                    ratios[42]=0.2/1000;

                    if(stop>2) stop=2;
                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }

            }
            else if(*m_accept_cabin.begin()==4)//只接受头等舱
            {
                if (not IsShortTraffic(start,end))
                {
                    //舱位中转比率表，key中的十位表示舱位，个位表示中转次数，value表示对应的比率
                    map<int,double> ratios;
                    ratios[40]=1.0;
                    ratios[41]=0.5;
                    ratios[30]=0.2;
                    ratios[31]=1.0/10;
                    ratios[10]=0.5/10;
                    ratios[42]=0.2/10;
                    ratios[32]=1.0/100;
                    ratios[11]=0.5/100;
                    ratios[12]=0.2/100;
                    ratios[43]=1.0/1000;
                    ratios[33]=0.5/1000;
                    ratios[13]=0.2/1000;

                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
                else
                {
                    map<int,double> ratios;
                    ratios[40]=1.0;
                    ratios[30]=0.9;
                    ratios[10]=0.7;
                    ratios[41]=1.0/10;
                    ratios[31]=0.9/10;
                    ratios[11]=0.7/10;
                    ratios[42]=1.0/100;
                    ratios[32]=0.9/100;
                    ratios[12]=0.7/100;

                    if(stop>2) stop=2;
                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
            }

        }
        else if(m_accept_cabin.size()==2)
        {
            if(*m_accept_cabin.begin()==1 and *m_accept_cabin.rbegin()==3)//只接受经济舱和商务舱
            {
                if (not IsShortTraffic(start,end))
                {
                    //舱位中转比率表，key中的十位表示舱位，个位表示中转次数，value表示对应的比率
                    map<int,double> ratios;
                    ratios[30]=1.0;
                    ratios[10]=0.5;
                    ratios[31]=0.2;
                    ratios[11]=1.0/10;
                    ratios[32]=0.5/10;
                    ratios[12]=0.2/10;
                    ratios[33]=1.0/100;
                    ratios[13]=0.5/100;
                    ratios[40]=0.2/100;
                    ratios[41]=1.0/1000;
                    ratios[42]=0.5/1000;
                    ratios[43]=0.2/1000;

                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
                else
                {
                    map<int,double> ratios;
                    ratios[30]=1.0;
                    ratios[10]=0.9;
                    ratios[31]=1.0/10;
                    ratios[11]=0.9/10;
                    ratios[32]=1.0/100;
                    ratios[12]=0.9/100;
                    ratios[40]=1.0/1000;
                    ratios[41]=0.5/1000;
                    ratios[42]=0.2/1000;

                    if(stop>2) stop=2;
                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
            }
            else if(*m_accept_cabin.begin()==3 and *m_accept_cabin.rbegin()==4)//只接受商务舱和头等舱
            {
                if (not IsShortTraffic(start,end))
                {
                    //舱位中转比率表，key中的十位表示舱位，个位表示中转次数，value表示对应的比率
                    map<int,double> ratios;
                    ratios[40]=1.0;
                    ratios[30]=0.5;
                    ratios[41]=0.2;
                    ratios[31]=1.0/10;
                    ratios[42]=0.5/10;
                    ratios[32]=0.2/10;
                    ratios[10]=1.0/100;
                    ratios[11]=0.5/100;
                    ratios[12]=0.2/100;
                    ratios[43]=1.0/1000;
                    ratios[33]=0.5/1000;
                    ratios[13]=0.2/1000;

                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
                else
                {
                    map<int,double> ratios;
                    ratios[40]=1.0;
                    ratios[30]=0.9;
                    ratios[10]=0.7;
                    ratios[41]=1.0/10;
                    ratios[31]=0.9/10;
                    ratios[11]=0.7/10;
                    ratios[42]=1.0/100;
                    ratios[32]=0.9/100;
                    ratios[12]=0.7/100;

                    if(stop>2) stop=2;
                    int key=final_cabin*10+stop;
                    cabin_stop_ratio=ratios[key];
                }
            }
        }
        else if(m_accept_cabin.size()==3)//3种舱位都能接受
        {
            if (not IsShortTraffic(start,end))
            {
                map<int,double> ratios;
                ratios[40]=1.0;
                ratios[30]=0.5;
                ratios[10]=0.2;
                ratios[41]=1.0/10;
                ratios[31]=0.5/10;
                ratios[11]=0.2/10;
                ratios[42]=1.0/100;
                ratios[32]=0.5/100;
                ratios[12]=0.2/100;
                ratios[43]=1.0/1000;
                ratios[33]=0.5/1000;
                ratios[13]=0.2/1000;

                int key=final_cabin*10+stop;
                cabin_stop_ratio=ratios[key];
            }
            else
            {
                map<int,double> ratios;
                ratios[40]=1.0;
                ratios[30]=0.9;
                ratios[10]=0.8;
                ratios[41]=1.0/10;
                ratios[31]=0.9/10;
                ratios[11]=0.8/10;
                ratios[42]=1.0/100;
                ratios[32]=0.9/100;
                ratios[12]=0.8/100;

                if(stop>2) stop=2;
                int key=final_cabin*10+stop;
                cabin_stop_ratio=ratios[key];
            }
        }
    }
    cur_ticket["prefer_ratios"]["cabin_stop_ratio"] = cabin_stop_ratio;
    prefer_ratio*=cabin_stop_ratio;


    double dept_time_ratio=1;
    if(!m_accept_dept_early)
    {
        int m=dept_times.size();
        int n=0;
        for (size_t i=0;i<dept_times.size();i++) if(dept_times[i]==2) n++;
        double q=0.2;
        dept_time_ratio=1-n*q/m;
        if((*m_accept_cabin.rbegin()==3 or *m_accept_cabin.rbegin()==4) and dept_time_ratio -1.0< -eps) dept_time_ratio*=0.1;
    }
    cur_ticket["prefer_ratios"]["dept_time_ratio"] = dept_time_ratio;
    cur_ticket["dept_times"] =Vector2Json(dept_times);
    prefer_ratio*=dept_time_ratio;

    double dest_time_ratio=1;
    if(!m_accept_dest_late)
    {
        int m=dest_times.size();
        int n=0;
        for (size_t i=0;i<dest_times.size();i++) if(dest_times[i]==2) n++;
        double q=0.2;
        dest_time_ratio=1-n*q/m;
        if((*m_accept_cabin.rbegin()==3 or *m_accept_cabin.rbegin()==4) and dest_time_ratio-1.0< -eps) dest_time_ratio*=0.1;
    }
    cur_ticket["prefer_ratios"]["dest_time_ratio"] = dest_time_ratio;
    cur_ticket["dest_times"] =Vector2Json(dest_times);
    prefer_ratio*=dest_time_ratio;

    double union_ratio=1;
    if(!m_prefer_union.empty())
    {
        int m=unions.size();
        int n=0;
        for (size_t i=0;i<unions.size();i++) if(unions[i]!=0) n++;
        double q=0.2;
        union_ratio=1-n*q/m;
    }
    cur_ticket["prefer_ratios"]["union_ratio"] = union_ratio;
    prefer_ratio*=union_ratio;

    double trans_interval_ratio=1;

    if(status==NOTCROSS and citys.empty())
    {
        if(m_city_country_map[start]=="日本")
        {
            citys.assign(dept_dest_times.size(),"20070");
        }
        else
        {
            citys.assign(dept_dest_times.size(),"10001");
        }
    }
    for(int i=0;i<dept_dest_times.size();i++)
    {
        if(i%2==1 and (i+1)<dept_dest_times.size())
        {
            if(!CheckTrans(citys[i],dept_dest_times[i],type,citys[i+1],dept_dest_times[i+1],type))
            {
                trans_interval_ratio=0.05;
                break;
            }
        }
    }
    cur_ticket["quality_ratios"]["trans_interval_ratio"] = trans_interval_ratio;
    quality_ratio*=trans_interval_ratio;

    double traffic_tool_ratio=1;
    if(type==TRAIN and m_accept_train==false) traffic_tool_ratio=0.5;
    if(type==BUS and m_accept_bus==false) traffic_tool_ratio=0.5;
    if(type == SHIP and m_accept_ship == false) traffic_tool_ratio=0.5;
    cur_ticket["prefer_ratios"]["traffic_tool_ratio"] = traffic_tool_ratio;
    prefer_ratio*=traffic_tool_ratio;

    vector<string> summary=tickets[0];

    double static_data_ratio=1;
    if(summary.size()>=6 and boost::lexical_cast<int>(summary[5])==1)
    {
        static_data_ratio=0.99999;
    }
    cur_ticket["prefer_ratios"]["static_data_ratio"] = static_data_ratio;
    prefer_ratio*=static_data_ratio;

    double transit_visa_ratio=1;
    if(summary.size()>=7 and boost::lexical_cast<int>(summary[6])==1)
    {
        transit_visa_ratio=0.05;
    }
    cur_ticket["quality_ratios"]["transit_visa_ratio"] = transit_visa_ratio;
    quality_ratio*=transit_visa_ratio;

    int train_bus_dur_boundry=5*60;
    if(m_price_prefer_ratio-price_prefer_ratio_economic>-eps)//大于等于
    {
        train_bus_dur_boundry=7*60;
    }
    else if(m_price_prefer_ratio-price_prefer_ratio_lightLuxury<eps)//小于等于
    {
        train_bus_dur_boundry=4*60;
    }
    double dur_ratio=1;
    if((type==TRAIN or type==BUS or type == SHIP) and time_cost> train_bus_dur_boundry)
    {
        dur_ratio=0.05/10;
        if(*m_accept_cabin.rbegin()==3 or *m_accept_cabin.rbegin()==4) dur_ratio*=1.0/1000;
    }
    //当火车或大巴在能接受的时间范围内,则直接将其舱位视为最佳舱位; 如此一个属性影响另一个属性设置的仅此一处
    if((type==TRAIN or type==BUS) and time_cost<= train_bus_dur_boundry and isBasic) { cabins.clear();cabins.push_back(4-4); }
    cur_ticket["quality_ratios"]["dur_ratio"] = dur_ratio;
    quality_ratio*=dur_ratio;

    vector<vector<double> > propertyM;
    propertyM.push_back(normalization(cabins, m_qualityM[0]));
    propertyM.push_back(V_stop);
    propertyM.push_back(V_dur);
    propertyM.push_back(normalization(dept_times, m_qualityM[3]));
    propertyM.push_back(normalization(dest_times, m_qualityM[4]));
    propertyM.push_back(normalization(airline_stars, m_qualityM[5]));
    propertyM.push_back(V_stop_transit);
    propertyM.push_back(V_airline_change);
    cur_ticket["propertys"] = Vector2Json(propertyM);
    //cout_matrix(propertyM);

    return propertyM;
}


// 拆开字符串形式的票
vector<vector<string>> Traffic::ParseTicketStr(const string &ticket_str)
{
    vector<vector<string>> tickets;
    vector<string> tmp = StrSplit(ticket_str, ";");
    for (int i = 0; i < tmp.size(); i++){
        tickets.push_back(StrSplit(tmp[i], "|"));
    }
    return tickets;
}

//将票还原成字符串
string Traffic::RecoveryTicketStr(const vector<vector<string>> &tickets)
{
    vector<string> tmp;
    for (int i = 0; i < tickets.size(); i++){
        tmp.push_back(StrJoin(tickets[i], "|"));
    }
    return StrJoin(tmp, ";");
}

//只在解析基本数据类型时，才将打分的详细信息记录下来
vector<vector<string>> Traffic::GetScore(const string& ticket_str, TicketType type, bool isBasic, bool debug)
{
    string ticket_mirror=ticket_str;
    TimezoneType status;
    CheckCrossTimezone(ticket_mirror,status);
    vector<vector<string>> ticket = ParseTicketStr(ticket_mirror);
    if(status==UNLEGAL)
    {
        return ticket;
    }

    Json::Value cur_ticket;
    cur_ticket["type"] = type;

    double raw_score=0;
    double quality_ratio=1;
    double prefer_ratio=1;
    vector<double> sub_scores;
    raw_score = CalScore(sub_scores, m_proportionV, m_qualityM, GetPropertyM(ticket,type,prefer_ratio,quality_ratio, cur_ticket,status,isBasic),quality_score_total);
    raw_score*= quality_score_total/score_total;
    if(raw_score>10) {_INFO("score booming:%f",raw_score);}
    double quality_score= raw_score*quality_ratio;
    double prefer_score = quality_score*prefer_ratio;
    if(ticket[0].size()>=8) ticket[0][7] = boost::lexical_cast<string>(quality_score);
    ticket[0][2] = boost::lexical_cast<string>(prefer_score);

    //将得分加入到原来的字符串
    cur_ticket["sub_scores"] = Vector2Json(sub_scores);
    cur_ticket["raw_score"] = raw_score;
    cur_ticket["quality_score"] = quality_score;
    cur_ticket["prefer_score"] = prefer_score;
    cur_ticket["raw_ticket"] = ticket_str;
    string unique_key=GetUniqueKey(ticket);
    if (debug) m_debug_res[unique_key] = cur_ticket;

    return ticket;
}

int Traffic::PriceAdjust(vector<string>& tickets)
{
    tr1::unordered_map<string,tr1::unordered_map<string,double>> categorized_ticket_price;

    for(int i=0;i<tickets.size();i++)
    {
        vector<string> basics;
        if(tickets[i].find("&")==string::npos)//飞机，火车，大巴等单程票,或者拼中转数据拆开后的单程票
        {
            basics.push_back(tickets[i]);
        }
        else //往返或联程票
        {
            boost::split(basics,tickets[i],boost::is_any_of("&"));
        }

        string category;
        string ticket_key;
        double price=-1;
        for(int j=0;j<basics.size();j++)
        {
            vector<vector<string>> ticket=ParseTicketStr(basics[j]);
            if(ticket[0][5]!="1") break;//非静态数据不参与价格调整;处理的是往返和联程的多段时,一段是非静态则整张票都不再调整了

            TicketType type;
            if(ticket[1].size()==7)
            {
                type=FLIGHT;
            }
            else if(ticket[1].size()==5)
            {
                type=TRAIN;
            }
            else if(ticket[1].size()==4)
            {
                type=BUS;
            }
            else if(ticket[1].size() == 6)
            {
                type = SHIP;
            }

            string stop=boost::lexical_cast<string>(ticket.size()-1-1);

            string corp_degrees;
            if(type==FLIGHT)
            {
                for(int j=1;j<ticket.size();j++)
                {
                    string airline = ticket[j].at(4);
                    corp_degrees+=m_airlines.count(airline) ? m_airlines[airline][0] : "4";
                }
            }

            string cabins;
            if(type!=BUS)
            {
                int cabin_index=4;//火车数据舱位的下标
                if(type==FLIGHT or type == SHIP) cabin_index=5;
                for(int j=1;j<ticket.size();j++)
                {
                    if (!ticket[j].at(cabin_index).empty()) {
                        int _cabin = boost::lexical_cast<int>(ticket[j].at(cabin_index));
                        if(_cabin<=5 and _cabin>=0) cabins+=boost::lexical_cast<string>(_cabin%5);
                    }
                }
            }

            category+=boost::lexical_cast<string>(type)+"|"+stop+"|"+corp_degrees+"|"+cabins;
            ticket_key+=GetUniqueKey(ticket);
            if(j<basics.size()-1){ category+="|";ticket_key+="|";}
            else if(j==basics.size()-1){ price=boost::lexical_cast<double>(ticket[0][0]);}
        }

        if(price>0) categorized_ticket_price[category][ticket_key]=price;
    }

    for(tr1::unordered_map<string,tr1::unordered_map<string,double>>::iterator iter=categorized_ticket_price.begin();iter!=categorized_ticket_price.end();iter++)
    {
        vector<double>prices;
        for(tr1::unordered_map<string,double>::iterator _iter=iter->second.begin();_iter!=iter->second.end();_iter++)
        {
            prices.push_back(_iter->second);
        }

        double average_price=accumulate(prices.begin(),prices.end(),0.0)/prices.size();

        for(tr1::unordered_map<string,double>::iterator _iter=iter->second.begin();_iter!=iter->second.end();_iter++)
        {
            if(_iter->second-average_price<0.000001){
                int price_adjust=static_cast<int>((average_price-_iter->second)*m_price_adjust_ratio);
                m_ticket_adjust_price[_iter->first]=_iter->second+price_adjust;
            }
        }
    }
}


//5转以上,过滤
//速度低于20公里每小时,过滤
//大洲内24小时以上,过滤
bool Traffic::Acceptable(const string& ticket)
{
    vector<string> stop_tmps;
    boost::split(stop_tmps,ticket,boost::is_any_of(":"),boost::token_compress_on);
    int stop_num=(stop_tmps.size()-1)/2-1;
    if(stop_num>4)
    {
        //_INFO("%s filtfilt:%d ",ticket.c_str(),stop_num);
        return false;
    }

    vector<string> tmps;
    boost::split(tmps,ticket,boost::is_any_of(";"),boost::token_compress_on);

    if(tmps.size()<=1)
    {
        //_INFO("%s filtfilt:%s ",ticket.c_str(), "data unexceptable");
        return false;
    }
    //取出首尾城市,以及出发和到达时间
    vector<string> depts;
    int dept_index=1;
    if(ticket.find(";;")!=-1) dept_index=2;
    boost::split(depts,tmps[dept_index],boost::is_any_of("|"));
    vector<string> dests;
    boost::split(dests,tmps[tmps.size()-1],boost::is_any_of("|"));
    if(depts.size()<4 or dests.size()<4)
    {
        //_INFO("%s filtfilt:%s ",ticket.c_str(), " data info less");
        return false;
    }
    string dept_time=depts[0];
    string dest_time=dests[1];
    vector<string> dept_station_city=StrSplit(depts[2], "#");
    vector<string> dest_station_city=StrSplit(dests[3], "#");
    if(dept_station_city.size()!=2 or dest_station_city.size()!=2)
    {
        //_INFO("%s filtfilt:%s ",ticket.c_str(), "city not found");
        return false;
    }
    string dept_city=dept_station_city[1];
    string dest_city=dest_station_city[1];

    //算出总的交通时长,单位分钟
    double time_cost = CalDur(dept_time,dest_time,dept_city,dest_city);
    if (m_city_continent.count(dept_city) && m_city_continent.count(dest_city) &&
            m_city_continent[dept_city] == m_city_continent[dest_city] && time_cost>24*60)
    {
        //_INFO("%s filtfilt:%s ",ticket.c_str(), "same continent time cost too high");
        return false;
    }
    int distance;

    bool has_mapInfo=GetCityDistance(dept_city,dest_city,distance);
    if(not has_mapInfo) return false;//城市信息缺少
    if(distance/(time_cost*60)<20.0*1000/3600)
    {
        //_INFO("%s filt:%s ",ticket.c_str(), "run too slow");
        return false;
    }

    return true;
}

bool costEfficientComp(const vector<string> & a,const vector<string> & b)
{
    return boost::lexical_cast<double>(a[3]) > boost::lexical_cast<double>(b[3]);
}

int Traffic::GetCostEfficientScore(tr1::unordered_map<string,vector<vector<string>>>& comparable_tickets,tr1::unordered_map<string,double> & cost_efficient_score_map ,Json::Value & debug_res)
{
    for(tr1::unordered_map<string,vector<vector<string>>>:: iterator map_iter= comparable_tickets.begin();map_iter!=comparable_tickets.end();map_iter++)
    {
        vector<vector<string>> & tickets=map_iter->second;
        if (tickets.size()==0) continue;
        for (vector<vector<string>>:: iterator it=tickets.begin(); it != tickets.end(); ++it){
            double costEfficient=pow(boost::lexical_cast<double>(it->at(0)),score_pow)*pow(10,6)/pow(boost::lexical_cast<double>(it->at(1)),m_price_prefer_ratio);
            it->push_back(boost::lexical_cast<string>(costEfficient));
        }
        int size=tickets.size();
        sort(tickets.begin(),tickets.end(),costEfficientComp);
        double rankFirst=boost::lexical_cast<double>(tickets[0][3]);
        double rankLast=boost::lexical_cast<double>(tickets[size-1][3]);
        for(int i=0; i<size; i++)
        {
            double current=boost::lexical_cast<double>(tickets[i][3]);
            double ratio=(current-rankLast+eps)/(rankFirst-rankLast+eps);//防止被除数为0
            if(ratio-1.0>eps) ratio=1.0;
            if(ratio-0<-eps) ratio=0;
            double costEfficientScore=ratio*(score_total-quality_score_total)*boost::lexical_cast<double>(tickets[i][0])/quality_score_total;
            string unique_key=tickets[i][2];
            cost_efficient_score_map[map_iter->first+"|"+unique_key]=costEfficientScore;
            debug_res[unique_key]["cost_efficient_score"]=costEfficientScore;
        }
    }
}



bool Traffic::ParseTrafficsParallel(Json::Value & data)
{
    vector<Json::Value> taskDatas=SplitTsv003Data(data);

    int thread_num=taskDatas.size();
    MyThreadPool* myThreadPool = new MyThreadPool;
    int threadStackSize=102400;
    if(thread_num<=0)
    {
        _ERROR("data is null");
        return true;
    }
    if (myThreadPool->open(thread_num,threadStackSize)!=0){
        delete myThreadPool;
        _ERROR("MyThreadPool open failed!,thread_num:%d",thread_num);
        return false;
    }
    myThreadPool->activate();
    std::vector<MJ::Worker*> jobs;
    for(int i=0; i< taskDatas.size(); i++)
    {
        Traffic* traffic = new Traffic(*this);//生成的对象必须是原对象的镜像,以保持偏好等的一致性
        traffic->m_trafficData=taskDatas[i];
        jobs.push_back((MJ::Worker*)traffic);
        myThreadPool->add_worker((MJ::Worker*)traffic);
    }
    myThreadPool->wait_worker_done(jobs);

    Json::Value mergedResult;
    for (int i=0;i<jobs.size();i++) {
        MergeResultPre((Traffic*)jobs[i]);
    }

    for (int i=0;i<jobs.size();i++) {
        Traffic* traffic = (Traffic*)jobs[i];
        MergeResult(mergedResult,traffic);
        delete traffic;
    }
    delete myThreadPool;

    m_trafficData=mergedResult;

    return true;
}


vector<Json::Value> Traffic::SplitTsv003Data(Json::Value & data)
{
    vector<Json::Value> taskDatas;

    //roundTripCombine实际上不存在
    vector<string> notBasic={"InterLine","roundTrip","combine","twiceCombine"};
    for(int i=0; i < notBasic.size(); i++)
    {
        if(data.isMember(notBasic[i])){
            Json::Value tmp;
            tmp[notBasic[i]]=data[notBasic[i]];
            taskDatas.push_back(tmp);
        }
    }
    vector<string> basic={"flight","bus","train","ship"};
    tr1::unordered_map<string,Json::Value> cityPairData;
    for(int i=0; i< basic.size(); i++)
    {
        string basicType=basic[i];
        vector<string> members_dept, members_dest;
        string dept_name, dest_name;
        if(data.isMember(basicType)){
            members_dept = data[basicType].getMemberNames();
            for (int j = 0; j < members_dept.size(); j++){
                dept_name = members_dept.at(j);
                Json::Value &dept = data[basicType][dept_name];
                members_dest = dept.getMemberNames();
                for (int k = 0; k < members_dest.size(); k++){
                    dest_name = members_dest.at(k);
                    Json::Value &str_list = dept[dest_name];  //航班字符串列表
                    for(int l=0; l<str_list.size(); l++)
                    {
                        if(str_list[l].asString().empty()) continue;
                        vector<vector<string>> ticket = ParseTicketStr(str_list[l].asString());
                        string dept_date = ticket[1][0].substr(0,8);
                        string am_pm = ticket[1][0].substr(9,2);
                        if(am_pm<"14") am_pm="am"; else am_pm="pm";
                        if(IsCrossContinent(dept_name,dest_name)) am_pm="ap";
                        string cityPairDate=dept_name+"_"+dest_name+"_"+dept_date+"_"+am_pm;
                        if(!cityPairData[cityPairDate].isMember(basicType)) cityPairData[cityPairDate][basicType]=Json::Value();
                        if(!cityPairData[cityPairDate][basicType].isMember(dept_name)) cityPairData[cityPairDate][basicType][dept_name]=Json::Value();
                        if(!cityPairData[cityPairDate][basicType][dept_name].isMember(dest_name)) cityPairData[cityPairDate][basicType][dept_name][dest_name]=Json::Value();
                        cityPairData[cityPairDate][basicType][dept_name][dest_name].append(str_list[l]);
                    }
                }
            }
        }
    }
    for(auto it=cityPairData.begin(); it!=cityPairData.end(); it++)
    {
        taskDatas.push_back(it->second);
    }

    /*
       Json::FastWriter fastWriter;
       for(int i=0; i< taskDatas.size(); i++)
       {
       cout<<fastWriter.write(taskDatas[i])<<endl;
       }
       */

    return taskDatas;
}


bool scoreMaxComp(const pair<string,double>& a,const pair<string,double>& b)
{
    return a.second>b.second;
}

void Traffic::MergeResultPre(Traffic* traffic)
{

    Json::Value debug = traffic->m_debug_res;
    vector<string> ticket_keys = debug.getMemberNames();
    for(int i=0; i< ticket_keys.size(); i++)
    {
        m_debug_res[ticket_keys[i]]=debug[ticket_keys[i]];
    }

    for(auto it=traffic->m_depts.begin(); it!=traffic->m_depts.end(); it++)
    {
        m_depts.insert(*it);
    }

    for(auto it=traffic->m_dests.begin(); it!=traffic->m_dests.end(); it++)
    {
        m_dests.insert(*it);
    }

    for(auto it=traffic->m_log_info.begin(); it != traffic->m_log_info.end(); it++)
    {
        if(it->first != "round_count")
        {
            m_log_info.insert(*it);
        }
        else
        {
            if(m_log_info.count("round_count") == 0)
            {
                m_log_info.insert(*it);
            }
            else
            {
                int round_count = boost::lexical_cast<int>(it->second) + boost::lexical_cast<int>(m_log_info["round_count"]);
                m_log_info["round_count"] = boost::lexical_cast<string>(round_count);
            }
        }
    }

    for(auto it=traffic->m_categorizedMaxScore.begin(); it!=traffic->m_categorizedMaxScore.end(); it++)
    {
        if(it->second<0 or it->second>score_total) continue;//得分超过10分的交通数据，将在MergeResult中打印出来

        if(m_categorizedMaxScore.find(it->first)==m_categorizedMaxScore.end())
        {
            m_categorizedMaxScore.insert(*it);
        }
        else
        {
            if(m_categorizedMaxScore[it->first]<it->second) m_categorizedMaxScore[it->first]=it->second;
        }
    }
    //最高分调整:下面的程序会确保m_scoreTimes和m_categorizedMaxScore的key一样多
    //  跨洲的对,最高分小于8分的对应到6.0~7.5之间;高于8分者不变
    //非跨洲的对,最高分小于3分的对应到1.0~2.5之间;高于3分者不变
    map<string,vector<pair<string,double>>> notCross;
    vector<pair<string,double>> Cross;
    for(auto it=m_categorizedMaxScore.begin(); it!= m_categorizedMaxScore.end(); it++)
    {
        m_scoreTimes[it->first]=1.0;//默认得分调整为原来的1.0倍率,即不变

        vector<string> tmp;
        boost::split(tmp,it->first,is_any_of("_"));
        bool cross=IsCrossContinent(tmp[0],tmp[1]);
        if(cross and it->second<8.0 )
        {
            Cross.push_back(*it);
        }
        else if(not cross and it->second<3.0 )
        {
            notCross[tmp[0]+"_"+tmp[1]].push_back(pair<string,double>(tmp[2]+"_"+tmp[3],it->second));//这里可以确保得到的tmp包含4个元素，故不用检查
        }
    }
    for(auto it=notCross.begin(); it!=notCross.end(); it++)
    {
        sort(it->second.begin(),it->second.end(),scoreMaxComp);
        double scoreDiff=(2.5-1.0)/it->second.size();
        for(int i=0; i<it->second.size(); i++) m_scoreTimes[it->first+"_"+it->second[i].first]=(2.5-i*scoreDiff)/m_categorizedMaxScore[it->first+"_"+it->second[i].first];
    }

    if(Cross.size()>0)
    {
        double scoreDiff=(7.5-6.0)/Cross.size();
        sort(Cross.begin(),Cross.end(),scoreMaxComp);
        for(int i=0; i<Cross.size(); i++)
        {
            m_scoreTimes[Cross[i].first]=(7.5-i*scoreDiff)/m_categorizedMaxScore[Cross[i].first];
        }
    }
}


void Traffic::normalizeBasicTicket(string& ticketStr,bool flag)
{
    if(ticketStr=="") return;//空字符串则不做任何处理
    vector<vector<string>> ticket = ParseTicketStr(ticketStr);
    string unique_key = GetUniqueKey(ticket);
    string category=GetCategory(ticket);
    if(category=="" or m_scoreTimes.find(category)==m_scoreTimes.end())
    {
        _ERROR("category(%s) not found",category.c_str());
    }
    double normalizedScore= boost::lexical_cast<double>(ticket[0][2])*m_scoreTimes[category];
    if(normalizedScore>(score_total+eps))
    {
        _ERROR("score booming!");
    }
    ticket[0][2]=boost::lexical_cast<string>(normalizedScore);
    if(flag)
    {
        m_debug_res[unique_key]["prefer_score"] = normalizedScore;
    }
    ticketStr=RecoveryTicketStr(ticket);
}


void Traffic::MergeResult(Json::Value & mergedResult, Traffic* traffic)
{
    set<string> notBasic={"InterLine","roundTrip","combine","twiceCombine"};
    set<string> basic={"flight","bus","train","ship"};

    Json::Value & data=traffic->m_trafficData;
    vector<string> dataTypes= data.getMemberNames();
    for(int i=0; i< dataTypes.size(); i++)
    {
        vector<string> members_dept, members_dest;
        string dept_name, dest_name;
        if(notBasic.find(dataTypes[i])!=notBasic.end())
        {
            data=data[dataTypes[i]];
            mergedResult[dataTypes[i]]=Json::Value();
            if(dataTypes[i]=="combine" or dataTypes[i]=="twiceCombine")
            {
                members_dept = data.getMemberNames();
                for (int _i = 0; _i < members_dept.size(); _i++){
                    dept_name = members_dept.at(_i);
                    mergedResult[dataTypes[i]][dept_name]=Json::Value();
                    Json::Value &dept = data[dept_name];
                    members_dest = dept.getMemberNames();
                    for (int j = 0; j < members_dest.size(); j++){
                        dest_name = members_dest.at(j);
                        mergedResult[dataTypes[i]][dept_name][dest_name]=Json::Value();
                        Json::Value & ticketArray = mergedResult[dataTypes[i]][dept_name][dest_name];
                        Json::Value &str_list = dept[dest_name];  //字符串列表
                        for (int k=0; k<str_list.size(); k++)
                        {
                            if(str_list[k]=="")
                            {
                                ticketArray.append(str_list[k]);
                            }
                            else
                            {
                                vector<string> strs = StrSplit(str_list[k].asString(), ";;");
                                if(strs.size()<2) continue;
                                vector<vector<string>> t=ParseTicketStr(strs[1]);
                                string deptDate=t[1][0].substr(0,8);
                                string am_pm = t[1][0].substr(9,2);
                                if(am_pm<"14") am_pm="am"; else am_pm="pm";
                                if(IsCrossContinent(dept_name,dest_name)) am_pm="ap";
                                string category=dept_name+"_"+dest_name+"_"+deptDate+"_"+am_pm;
                                if(m_scoreTimes.find(category)==m_scoreTimes.end())
                                {
                                    _ERROR("mergeResult():category not found");
                                    continue;
                                }

                                vector<string> summary = StrSplit(strs[0],"|");
                                double normalizedScore= boost::lexical_cast<double>(summary[1])*m_scoreTimes[category];
                                if(normalizedScore>(score_total+eps))
                                {
                                    _ERROR("mergeResult():score booming!");
                                }
                                summary[1]=boost::lexical_cast<string>(normalizedScore);
                                strs[0] = StrJoin(summary,"|");
                                ticketArray.append(StrJoin(strs, ";;"));
                            }
                        }
                    }
                }
            }
            else if(dataTypes[i]=="roundTrip")
            {
                members_dept = data.getMemberNames();
                for (int _i = 0; _i < members_dept.size(); _i++){
                    dept_name = members_dept.at(_i);
                    mergedResult[dataTypes[i]][dept_name]=Json::Value();
                    Json::Value &dept = data[dept_name];
                    members_dest = dept.getMemberNames();
                    for (int j = 0; j < members_dest.size(); j++){
                        dest_name = members_dest.at(j);
                        mergedResult[dataTypes[i]][dept_name][dest_name]=Json::Value();
                        Json::Value & ticketArray = mergedResult[dataTypes[i]][dept_name][dest_name];
                        Json::Value &str_list = dept[dest_name];  //字符串列表  [去程航班1&回程航班1&&回程航班2, ....]
                        for(int k=0; k<str_list.size(); k++)
                        {
                            if(str_list[k]=="")
                            {
                                ticketArray.append(str_list[k]);
                            }
                            else
                            {
                                vector<string>tickets;
                                string ticketStr=str_list[k].asString();
                                boost::split(tickets,ticketStr,is_any_of("&"));//magic skill, "&&"之间将会被认为存在一个空字符串;使用"&"连接则可还原字符串
                                for(int l=0; l<tickets.size(); l++)
                                {
                                    normalizeBasicTicket(tickets[l]);
                                }
                                ticketArray.append(StrJoin(tickets, "&"));
                            }
                        }
                    }
                }
            }
            else if(dataTypes[i]=="InterLine")
            {
                vector<string >members = data.getMemberNames();
                for (int j = 0; j < members.size(); j++){
                    string dept_dest = members.at(j);
                    mergedResult[dataTypes[i]][dept_dest]=Json::Value();
                    Json::Value & ticketArray = mergedResult[dataTypes[i]][dept_dest];
                    Json::Value &str_list = data[dept_dest];  //字符串列表  [去程航班1&回程航班1&&回程航班2, ....]
                    for(int k=0; k<str_list.size(); k++)
                    {
                        if(str_list[k]=="")
                        {
                            ticketArray.append(str_list[k]);
                        }
                        else
                        {
                            vector<string>tickets;
                            string ticketStr=str_list[k].asString();
                            boost::split(tickets,ticketStr,is_any_of("&"));//magic skill, "&&"之间将会被认为存在一个空字符串;使用"&"连接则可还原字符串
                            for(int l=0; l<tickets.size(); l++)
                            {
                                normalizeBasicTicket(tickets[l]);
                            }
                            ticketArray.append(StrJoin(tickets, "&"));
                        }
                    }
                }
            }
        }
        else if(basic.find(dataTypes[i])!=basic.end())
        {
            string basicType=dataTypes[i];
            vector<string> members_dept, members_dest;
            string dept_name, dest_name;

            members_dept = data[basicType].getMemberNames();
            for (int j = 0; j < members_dept.size(); j++){
                dept_name = members_dept.at(j);
                Json::Value &dept = data[basicType][dept_name];
                members_dest = dept.getMemberNames();
                for (int k = 0; k < members_dest.size(); k++){
                    dest_name = members_dest.at(k);
                    Json::Value &str_list = dept[dest_name];  //basic交通列表
                    if(!mergedResult.isMember(basicType)) mergedResult[basicType]=Json::Value();
                    if(!mergedResult[basicType].isMember(dept_name)) mergedResult[basicType][dept_name]=Json::Value();
                    if(!mergedResult[basicType][dept_name].isMember(dest_name)) mergedResult[basicType][dept_name][dest_name]=Json::Value();
                    for(int l=0; l<str_list.size(); l++)
                    {
                        string ticketStr=str_list[l].asString();
                        normalizeBasicTicket(ticketStr);
                        mergedResult[basicType][dept_name][dest_name].append(ticketStr);
                    }
                }
            }
        }
        else
        {

        }
    }
}
