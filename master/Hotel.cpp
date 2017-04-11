#include "Hotel.hpp"
using namespace MJ;

const double price_prefer_ratio_economic=0.3;
const double price_prefer_ratio_comfortable=2.0;
const double price_prefer_ratio_lightLuxury=8.0;
const double price_prefer_ratio_luxury=20.0;

//静态变量初始化
CONFIG Hotel:: m_ips;
tr1::unordered_map<string, double> Hotel:: m_dis_map;
tr1::unordered_map<string, Json::Value> Hotel:: m_hotel_map;
tr1::unordered_map<string, double> Hotel:: m_city_hotel_statis_price_map; //statis 统计性
pair<double,double> Hotel:: m_city_map_info;

bool Hotel::Init()
{
    //读取配置文件
    ConfigParser config;
    m_ips = config.read(IP);
    cout << m_ips << endl;

    //读取数据库
    LoadBaseData(m_ips["base_data"]["ip"], m_ips["base_data"]["db"], m_ips["base_data"]["usr"], m_ips["base_data"]["passwd"]);

    return true;
}


//读取basedata.
void Hotel::LoadBaseData(const string &host, const string &db, const string &usr, const string &passwd)
{
    Mysql mysql(host, db, usr, passwd);
    if (!mysql.connect()){
        assert(false);
    }
    _INFO("connect %s success!", db.c_str());

    _INFO("load m_hotel_map begin");
    //酒店基础信息
    string sql = "select uid,map_info,star,grade,brand_tag,hotel_name,hotel_type,first_img from hotel";
    if (mysql.query(sql)){
        MYSQL_RES *res = mysql.use_result();

        MYSQL_ROW row;
        if(res){
            while (row = mysql.fetch_row(res)){
                Json::Value hotel_info;
                hotel_info["map_info"] = row[1];
                hotel_info["star"] = row[2];
                hotel_info["grade"] = row[3];
                hotel_info["brand_tag"] = row[4];
                hotel_info["hotel_name"] = row[5];
                hotel_info["hotel_type"] = row[6];
                hotel_info["no_img"] = string(row[7])==""?"1":"0";//有无图片的标志
                m_hotel_map[row[0]] = hotel_info;
            }
        }
        mysql.free_result(res);
    }
    _INFO("load m_hotel_map end");

    _INFO("load m_city_hotel_statis_price_map begin");
    //城市酒店价格统计信息
    sql = "select cid, type, price from city_hotel_static_price where checkin=20170000 and price>0.000001";
    if (mysql.query(sql)){
        MYSQL_RES *res = mysql.use_result();

        MYSQL_ROW row;
        if(res){
            while (row = mysql.fetch_row(res)){
                double statis_price=boost::lexical_cast<double>(row[2]);
                if(string(row[1])=="13" and statis_price>500) statis_price=500;
                if(string(row[1])=="3" and statis_price>750) statis_price=750;
                if(string(row[1])=="4" and statis_price>1000) statis_price=1000;
                if(string(row[1])=="5" and statis_price>1250) statis_price=1250;
                string city_star=string(row[0])+"_"+row[1];
                m_city_hotel_statis_price_map[city_star]=statis_price;
            }
        }
        mysql.free_result(res);
    }
    _INFO("load m_city_hotel_statis_price_map end");

    _INFO("load m_dis_map begin");
    //酒店距离信息
    if (mysql.query("select hotel.uid, hotel.map_info as hotel_map_info, city.map_info as city_map_info from hotel inner join city on hotel.city_mid=city.id where city.newProduct_status='Open' and hotel.map_info is not null and hotel.map_info!='NULL' and city.map_info is not null and city.map_info!='NULL'")){
        MYSQL_RES *res = mysql.use_result();
        MYSQL_ROW row;
        if(res){
            vector<string> mapinfo1;
            vector<string> mapinfo2;
            while (row = mysql.fetch_row(res)){
                if (row[1] && row[2]){
                    mapinfo1.clear();
                    mapinfo2.clear();
                    boost::split(mapinfo1, row[1], boost::is_any_of(", "),boost::token_compress_on);
                    boost::split(mapinfo2, row[2], boost::is_any_of(", "),boost::token_compress_on);
                    double lat1 = boost::lexical_cast<double>(mapinfo1[1]);
                    double lng1 = boost::lexical_cast<double>(mapinfo1[0]);
                    double lat2 = boost::lexical_cast<double>(mapinfo2[1]);
                    double lng2 = boost::lexical_cast<double>(mapinfo2[0]);
                    double dis = GetDistance(lat1, lng1, lat2, lng2);
                    m_dis_map[row[0]] = dis;
                }
            }
            double lat = boost::lexical_cast<double>(mapinfo2[1]);
            double lng = boost::lexical_cast<double>(mapinfo2[0]);
            m_city_map_info.first = lat;
            m_city_map_info.second = lng;
        }
        mysql.free_result(res);
    }
    _INFO("m_dis_map size:%d",m_dis_map.size());
    _INFO("load m_dis_map end");

    assert(!m_hotel_map.empty() && !m_city_hotel_statis_price_map.empty() && !m_dis_map.empty());
}


bool hotelPriceComp(const Json::Value &h1, const Json::Value &h2)
{
    return h1["room"][0u]["price"].asInt() < h2["room"][0u]["price"].asInt();
}

//从大到小
bool hotelScoreComp(const Json::Value &h1, const Json::Value &h2)
{
    return h1["preferScore"].asDouble() > h2["preferScore"].asDouble();
}

//选择最终的酒店
int Hotel::HotelSelect()
{
    if(m_hotelData.size()==0) return -1;

    vector<Json::Value> hotels_total_vector_mirror;
    for (int i = 0; i < m_hotelData.size(); ++i){
        hotels_total_vector_mirror.push_back(m_hotelData[i]);
    }

    //获取候选票
    vector<Json::Value> candidate;

    sort(hotels_total_vector_mirror.begin(),hotels_total_vector_mirror.end(),hotelScoreComp);
    double max_score=hotels_total_vector_mirror[0]["preferScore"].asDouble();

    sort(hotels_total_vector_mirror.begin(),hotels_total_vector_mirror.end(),hotelPriceComp);
    double last_max_score=-1.0;
    for(int i=0; i<hotels_total_vector_mirror.size(); i++)
    {
        string uid=hotels_total_vector_mirror[i]["id"].asString();
        m_debug_res[uid]["category"]="-";//默认被丢弃
        double score=hotels_total_vector_mirror[i]["preferScore"].asDouble();
        if(score>last_max_score){
            last_max_score=score;
            if(not m_star_prefer_13_only)
            {
                if(max_score-1.99999>eps and score-1.99999<eps) continue;//有大于2分的票,则2分以下的票全丢弃
                if(max_score-4.99999>eps and score-4.99999<eps) continue;//有大于5分的票,则5分以下的票全丢弃
            }
            candidate.push_back(hotels_total_vector_mirror[i]);
            m_debug_res[uid]["category"]="candidate";//设置此票为预选
        }
    }

    if(m_display_all <= 0)
    {
        tr1::unordered_set<string> hotel_set;
        for (int i = 0; i < candidate.size(); ++i){
            hotel_set.insert(candidate[i]["id"].asString());
        }

        Json::Value::Members ids = m_debug_res.getMemberNames();
        for (int i = 0; i < ids.size(); ++i){
            string id = ids[i];
            if (hotel_set.count(id) == 0){
                m_debug_res.removeMember(id);
            }
        }
    }

    int selected=0;
    last_max_score=-1;

    double scoreHlimit=10.0;
    double preferScoreTimes=scoreHlimit/max_score;
    for(int i=0; i<candidate.size();i++)
    {
        //当m_price_prefer_ratio<0时,其将不被使用，而转用用户隐藏价格偏好设定规则
        double ratio=m_price_prefer_ratio>0?m_price_prefer_ratio:m_star_price_prefer_ratios[candidate[i]["as_star"].asInt()];
        double final_score=pow(candidate[i]["preferScore"].asDouble()*preferScoreTimes,ratio)/candidate[i]["room"][0u]["price"].asInt();
        if(final_score>last_max_score)
        {
            selected=i;
            last_max_score=final_score;
        }
    }
    string uid_selected=candidate[selected]["id"].asString();
    m_debug_res[uid_selected]["category"]="selected";//设置此票为最终选中的票
    m_hotelData.clear();
    m_hotelData.append(candidate[selected]);
}

string Hotel::csv010(const string &query,const string & qid,string & other_params,string ptid)
{
    m_log_info.insert(pair<string,string>("qid",qid));
    m_log_info.insert(pair<string,string>("rq_type","csv010"));
    Json::Reader reader;
    Json::Value queryobj;
    reader.parse(query, queryobj);
    m_cid = queryobj.get("cid", "").asString();
    m_ptid = ptid;
    m_budget_ratio=1;
    int budgetReal = queryobj.get("budgetReal",-1).asInt();
    int budgetPredict = queryobj.get("budgetPredict",-1).asInt();
    if(budgetReal>0 and budgetPredict >0)
    {
        double budget_ratio=budgetReal*1.0/budgetPredict;
        if(budget_ratio<0.5)
        {
            budget_ratio=0.5;
        }
        else if(budget_ratio>5)
        {
            budget_ratio=5;
        }
        m_budget_ratio=budget_ratio;
    }

    m_display_all=queryobj.get("display_all",0).asInt();
    //解析偏好
    m_prefer=Json::nullValue;
    if (queryobj.isMember("prefer")){
        m_prefer = queryobj["prefer"];
    }

    SetDefaultQualityM();
    //判断是否是debug请求
    bool debug = false;
    if (queryobj.isMember("debug")){
        debug = true;
        SetQualityM(queryobj["debug"]);
    }

    queryobj["debug"] = 1;
    if(!queryobj.isMember("occ")) queryobj["occ"] = 2;
    Json::FastWriter fastWriter;
    std::string jsonstr = fastWriter.write(queryobj);
    std::size_t n = jsonstr.size();
    if (jsonstr.at(n-1) == '\n'){
        jsonstr = jsonstr.substr(0, n-1);
    }

    //转发请求
    DateTime t1 = DateTime::Now();
    SocketClient *client = new SocketClient();
    client->init("10.10.135.140:"+m_ips["inner_proxy"]["port"], 10000000);
    string real_req = "hotelsearch?type=hsv003&qid="+qid+"&query=" + MJ::UrlEncode(jsonstr)+"&"+other_params+"&ptid="+ptid;
    ServerRst server_rst;
    client->getRstFromHost(real_req, server_rst, 0);
    string result = server_rst.ret_str;
    delete client;
    DateTime t2 = DateTime::Now();

    //result = "{\"data\":{\"hotel\":[{\"addr\":\"\",\"brand\":\"3\",\"custom\":0,\"checkin\":\"20170329\",\"checkout\":\"20170330\",\"coord\":\"116.43749356269836,39.90765410914343\",\"id\":\"ht30554134\",\"lname\":\"长富宫饭店\",\"name\":\"长富宫饭店\",\"room\":[{\"id\":\"{\\\"checkin\\\":\\\"20170329\\\",\\\"checkout\\\":\\\"20170330\\\",\\\"md5\\\":\\\"89a74a16897cc749\\\",\\\"need\\\":1,\\\"norm_room\\\":\\\"双人房\\\",\\\"occ\\\":2,\\\"source\\\":\\\"booking\\\",\\\"uid\\\":\\\"ht30554134\\\"}\",\"num\":0,\"occ\":2,\"price\":755,\"type\":\"双人房\"}],\"score\":8.5,\"star\":5,\"tel\":\"\"},{\"addr\":\"\",\"brand\":4,\"custom\":0,\"checkin\":\"20170329\",\"checkout\":\"20170330\",\"coord\":\"116.41119182109833,39.92352357431143\",\"id\":\"ht30553595\",\"lname\":\"北京华侨大厦\",\"name\":\"北京华侨大厦\",\"room\":[{\"id\":\"{\\\"checkin\\\":\\\"20170329\\\",\\\"checkout\\\":\\\"20170330\\\",\\\"md5\\\":\\\"d8952e072c9fe1ba\\\",\\\"need\\\":1,\\\"norm_room\\\":\\\"双人房\\\",\\\"occ\\\":2,\\\"source\\\":\\\"booking\\\",\\\"uid\\\":\\\"ht30553595\\\"}\",\"num\":0,\"occ\":2,\"price\":461,\"type\":\"双人房\"}],\"score\":7.8,\"star\":5,\"tel\":\"\"}]},\"error\":{\"error_id\":0,\"error_str\":\"\"}}";
    //_INFO("%s",result.c_str());

    Json::Value res_json;
    reader.parse(result, res_json);

    Json::Value &data = res_json["data"];
    Json::Value error = res_json["error"];

    m_log_info.insert(pair<string,string>("is_success","1"));
    if (error["error_id"].asInt() != 0){
        _INFO("fail");
        m_log_info["is_success"] = "0";
        logInfo2Str(m_log_info);
        return result;
    }

    //获取酒店列表
    Json::Value &hotels = data["hotel"];
    if (hotels.empty()){
        _INFO("fail");
        m_log_info["is_success"] = "0";
        logInfo2Str(m_log_info);
        return  result;
    }
    m_isPrivate = (data["private"].asInt() == 1);
    m_hotelData = hotels;
    // 打分
    GetScores();

    //筛选酒店
    HotelSelect();
    hotels = m_hotelData;

    // 重组结果返回
    if(debug)
    {
        res_json["debug_info"]=m_debug_res;
    }
    string res = fastWriter.write(res_json);
    // 返回结果
    DateTime t3 = DateTime::Now();

    double getDataCost=(t2-t1).GetTotalSeconds();
    _INFO("%f,%f",getDataCost, (t3-t2).GetTotalSeconds());
    string rq_consume = boost::lexical_cast<string>(getDataCost);
    string process_consume = boost::lexical_cast<string>((t3-t2).GetTotalSeconds());
    m_log_info.insert(pair<string,string>("rq_consume",rq_consume));
    m_log_info.insert(pair<string,string>("process_consume",process_consume));
    logInfo2Str(m_log_info);
    if(getDataCost >3.0) logException("ex21000",m_uid.c_str(),m_csuid.c_str(),m_qid.c_str(),(string("time cost ")+ boost::lexical_cast<string>(getDataCost)).c_str());
    return res;
}

string Hotel::csv020(const string &query,const string & qid, string & other_params,string ptid)
{
    Json::Reader reader;
    Json::Value queryobj;
    reader.parse(query, queryobj);

    m_display_all = queryobj.get("display_all",0).asInt();

    //解析偏好
    m_prefer= Json::nullValue;
    if(queryobj.isMember("prefer")){
        m_prefer = queryobj["prefer"];
    }

    SetDefaultQualityM();

    //判断是否有debug请求
    bool debug = false;
    if(queryobj.isMember("debug")){
        debug = true;
        SetQualityM(queryobj["debug"]);
    }

    queryobj["debug"] = 1;
    if(queryobj.isMember("hotelList"))
    {
        for(int i=0; i<queryobj["hotelList"].size(); i++)
        {
            if(not queryobj["hotelList"][i].isMember("occ") or queryobj["hotelList"][i]["occ"].asInt()<0)
                queryobj["hotelList"][i]["occ"]=2;
        }
    }
    Json::FastWriter fastWriter;
    std::string jsonstr = fastWriter.write(queryobj);
    std::size_t n = jsonstr.size();
    if(jsonstr.at(n-1) == '\n'){
        jsonstr = jsonstr.substr(0,n-1);
    }

    //转发请求
    DateTime t1 = DateTime::Now();
    SocketClient *client = new SocketClient();
    client->init("10.10.135.140:"+m_ips["inner_proxy"]["port"], 10000000);
    string real_req = "hotelsearch?type=h105&qid="+qid+"&query="+MJ::UrlEncode(jsonstr)+"&"+other_params+"&ptid="+ptid;
    ServerRst server_rst;
    client->getRstFromHost(real_req,server_rst,0);
    string result = server_rst.ret_str;
    delete client;
    DateTime t2 = DateTime::Now();

    Json::Value res_json;
    reader.parse(result,res_json);

    Json::Value &data = res_json["data"]["hotelList"];
    Json::Value error = res_json["error"];

    if(error["error_id"].asInt() != 0){
        _INFO("fail");
        return result;
    }

    GetScoresParallel(data);
    res_json["data"]["hotelList"] = m_hotelData;

    if(debug)
    {
        res_json["debug_info"] = m_debug_res;
    }

    string res = fastWriter.write(res_json);
    DateTime t3 = DateTime::Now();
    _INFO("%f,%f",(t2-t1).GetTotalSeconds(),(t3-t2).GetTotalSeconds());
    return res;
}

//设置默认的比例系数，和品质矩阵
int Hotel::SetDefaultQualityM()
{
    if (m_proportionV.empty()){
        m_proportionV.push_back(4);  //星级
        m_proportionV.push_back(3);  //评分
        m_proportionV.push_back(1);  //品牌
        m_proportionV.push_back(2);  //距离
    }

    if (m_qualityM.empty()){
        double stars[] = {5, 4, 3, 2, 1};//星级未知时，认为其为1星级酒店
        vector<double> quality1(stars, stars + 5);
        vector<double> quality2(1, 1);
        double brands[] = {5, 4, 3, 2, 1};
        vector<double> quality3(brands, brands + 5);
        vector<double> quality4(1, 1);

        m_qualityM.push_back(quality1);
        m_qualityM.push_back(quality2);
        m_qualityM.push_back(quality3);
        m_qualityM.push_back(quality4);
    }
}

//如果debug带有比例系数，则设置
int Hotel::SetQualityM(const Json::Value &debug)
{
    cout << "SetQualityM" << endl;
    if (debug.empty()) return -1;
    if (debug.isMember("proportion") && !debug["proportion"].empty()){
        m_proportionV.clear();
        Json::Value tmp = debug["proportion"];
        for (int i = 0; i < tmp.size(); i++)
            m_proportionV.push_back(tmp[i].asDouble());
    }

    cout_matrix(m_proportionV);
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
    cout_matrix(m_qualityM);
}

bool hotelDisComp(const pair<string,double> &p1 , const pair<string,double> &p2)
{
    return p1.second<p2.second;
}

bool hotelGradeComp(const pair<string,double> &p1 , const pair<string,double> &p2)
{
    return p1.second>p2.second;
}

//品质得分计算数据初始化
bool Hotel::GetScores()
{
    Json::Value hotel_prefer = m_prefer.get("hotel", Json::nullValue);
    std::set<int> prefer_stars;
    prefer_stars.insert(3);
    prefer_stars.insert(4);
    bool single_select_4=false;
    m_star_prefer_13_only=false;

    m_price_prefer_ratio=-1.0;
    if(m_prefer.isMember("global"))
    {
        m_price_prefer_ratio=m_prefer["global"].get("price_prefer_ratio",-1.0).asDouble();
    }

    m_star_price_prefer_ratios.clear();
    m_star_price_prefer_ratios[1]=price_prefer_ratio_economic;
    m_star_price_prefer_ratios[2]=price_prefer_ratio_economic;
    m_star_price_prefer_ratios[3]=price_prefer_ratio_economic;
    m_star_price_prefer_ratios[4]=price_prefer_ratio_comfortable;
    m_star_price_prefer_ratios[5]=price_prefer_ratio_lightLuxury;
    if(m_prefer.isMember("flight") && m_prefer["flight"].isMember("class"))
    {
        //当用户偏好舱位中有商务舱或头等舱时,5星级酒店偏好奢侈型...
        for (int i=0; i< m_prefer["flight"]["class"].size(); i++)
        {
            if(m_prefer["flight"]["class"][i].asInt()==3 or m_prefer["flight"]["class"][i].asInt()==4 )
            {
                for(int j=1; j<5; j++) m_star_price_prefer_ratios[j]=m_star_price_prefer_ratios[j+1];
                m_star_price_prefer_ratios[5]=price_prefer_ratio_luxury;
                break;
            }
        }
    }

    if(hotel_prefer != Json::nullValue && hotel_prefer.isMember("type")){
        std::set<int> _prefer_stars;
        Json::Value type_prefer = hotel_prefer["type"];
        int maxSelectStar=1;
        for (int i = 0; i < type_prefer.size(); i++){
            int selectStar=type_prefer[i].asInt();
            if (selectStar == 13){
                _prefer_stars.insert(1);
                _prefer_stars.insert(2);
                if(maxSelectStar<2) maxSelectStar=2;
            }
            else if(selectStar<=5 and selectStar>=3)
            {
                _prefer_stars.insert(selectStar);
                if(maxSelectStar<selectStar) maxSelectStar=selectStar;
            }
        }
        assert(maxSelectStar>=1 and maxSelectStar<=5);
        for(int j=1; j<maxSelectStar; j++) m_star_price_prefer_ratios[j]=m_star_price_prefer_ratios[j+1];
        for(int j=5; j>maxSelectStar; j--) m_star_price_prefer_ratios[j]=m_star_price_prefer_ratios[j-1];

        if(type_prefer.size()==1)
        {
            int selectOnly=type_prefer[0u].asInt();

            if(selectOnly==13)//13,左开右闭[1,3),即表达1星和2星
            {
                m_star_prefer_13_only=true;
            }
            else if(selectOnly==3)
            {
                _prefer_stars.insert(4);
            }
            else if(selectOnly==4)
            {
                _prefer_stars.insert(5);
                single_select_4=true;
            }
        }
        if(not _prefer_stars.empty()) prefer_stars=_prefer_stars;
    }
    //判断库中是否存在私有库酒店，如果存在私有库酒店并且私有库酒店满足用户偏好，推荐给用户的酒店从这部分中选择
    //否则按照原来的推荐进行推荐
    if(m_isPrivate)
    {
        Json::Value is_prefer_hotels;//筛选出用户偏好的私有酒店
        for(int i = 0; i < m_hotelData.size(); ++i)
        {
            Json::Value tmp = m_hotelData[i];
            if(tmp["custom"].asInt() == 3)//当前酒店是私有库酒店
            {
                int star = tmp["star"].asInt();
                if(prefer_stars.count(star) == 1)//满足用户的偏好
                {
                    is_prefer_hotels.append(tmp);
                }
            }
        }
        if(is_prefer_hotels != Json::nullValue)
        {
            m_hotelData = is_prefer_hotels;
        }
        else//私有库酒店中没有满足用户偏好
        {
            m_isPrivate = false;
        }
    }
    vector<pair<string,double> >tmp_dis_map;
    vector<pair<string,double> >tmp_grade_map;
    for (int i = 0; i < m_hotelData.size(); i++){
        string uid = m_hotelData[i]["id"].asString();
        if (m_hotel_map.count(uid) == 1 and m_dis_map.count(uid)==1){
            tmp_dis_map.push_back(make_pair(uid,m_dis_map[uid]));
            double grade = m_hotel_map[uid].isMember("grade") ? boost::lexical_cast<double>(m_hotel_map[uid]["grade"].asString()) : 0;
            m_hotelData[i]["score"]=boost::lexical_cast<string>(grade);
            tmp_grade_map.push_back(make_pair(uid,grade));
        }
        else //如果静态加载的数据无法得到距离和分数信息，则从hotel数据中获得
        {
            vector<string> mapinfo;
            string mapinfoStr=m_hotelData[i]["coord"].asString();
            boost::split(mapinfo, mapinfoStr, boost::is_any_of(","),boost::token_compress_on);
            double dis=1000*100;//默认是一个很远的距离
            if(mapinfo.size()==2)
            {
                boost::trim(mapinfo[1]);
                boost::trim(mapinfo[0]);
                double lat1 = boost::lexical_cast<double>(mapinfo[1]);
                double lng1 = boost::lexical_cast<double>(mapinfo[0]);
                double lat2 = m_city_map_info.first;
                double lng2 = m_city_map_info.second;
                double dis = GetDistance(lat1,lng1,lat2,lng2);
            }
            tmp_dis_map.push_back(make_pair(uid,dis));
            string gradeStr = m_hotelData[i]["score"].asString();
            double grade=0;
            if(gradeStr=="")
            {
                if ( not m_isPrivate)
                {
                    grade=0;
                }
                else if(m_isPrivate)
                {
                    grade=8.0; //私有酒店库都没有评分,赋值为8.0,保证其不备排除
                }
                m_hotelData[i]["score"]=boost::lexical_cast<string>(grade);
            }
            else
            {
                grade = boost::lexical_cast<double>(gradeStr);
            }
            tmp_grade_map.push_back(make_pair(uid,grade));
        }
    }
    //_INFO("tmp_dis_map,%d",tmp_dis_map.size());
    //_INFO("tmp_grade_map,%d",tmp_grade_map.size());

    tr1::unordered_map<string, double> hotel_dis_order;
    sort(tmp_dis_map.begin(),tmp_dis_map.end(), hotelDisComp);
    for(size_t i=0;i<tmp_dis_map.size();i++)
    {
        hotel_dis_order[tmp_dis_map[i].first]=(i+1.0)/tmp_dis_map.size();
        //_INFO("hotel_dis_order,%s,%f,%f",tmp_dis_map[i].first.c_str(),tmp_dis_map[i].second,(i+1.0)/tmp_dis_map.size());
    }

    tr1::unordered_map<string, double> hotel_grade_order;
    sort(tmp_grade_map.begin(),tmp_grade_map.end(), hotelGradeComp);
    for(size_t i=0;i<tmp_grade_map.size();i++)
    {
        hotel_grade_order[tmp_grade_map[i].first]=(i+1.0)/tmp_grade_map.size();
        //_INFO("hotel_grade_order,%s,%f,%f",tmp_grade_map[i].first.c_str(),tmp_grade_map[i].second,(i+1.0)/tmp_grade_map.size());
    }

    //属性矩阵
    vector<vector<double>> propertyM;
    vector<double> property1(5, 0), property2(1), property3(5, 0), property4(1);

    for (int i = 0; i < m_hotelData.size(); i++){
        string uid = m_hotelData[i]["id"].asString();
        //if (!m_isPrivate && m_hotel_map.count(uid) == 0){
            //m_hotelData[i]["qualityScore"] = 0;
            //m_hotelData[i]["preferScore"] = 0;
            //continue;
        //}

        Json::Value cur_ticket;
        //直接对总得分进行一个比例的惩罚
        double prefer_ratio=1;
        double quality_ratio=1;

        Json::Value info = m_hotel_map[uid];//非私有库酒店信息从静态数据中得到
        info["ptid"]="";
        int custom = m_hotelData[i]["custom"].asInt();//处理不满足用户偏好的非私有库酒店
        if(m_isPrivate || custom == 3)//私有库信息从hsv003返回的数据得到
        {
            info["brand_tag"] = m_hotelData[i]["brand"].asString();
            info["grade"] = m_hotelData[i]["score"].asString();
            info["hotel_name"] = m_hotelData[i]["lname"].asString();
            info["hotel_type"] = "0";
            info["map_info"] = m_hotelData[i]["coord"].asString();
            info["no_img"] = "0";
            info["star"] = m_hotelData[i]["star"].asDouble();
            info["ptid"] = m_ptid;
        }

        //属性3:品牌
        char brand = '0';
        if(!m_isPrivate)
        {
            brand = info.isMember("brand_tag") ? (info["brand_tag"].asString().empty() ? '0' : info["brand_tag"].asString()[0]) : '0';
            property3.assign(5, 0);
            if (brand == '4') property3[3] = 1;
            else if (brand == '3') property3[2] = 1;
            else if (brand == '2') property3[1] = 1;
            else if (brand == '1') property3[0] = 1;
            else property3[4] = 1;
            m_hotelData[i]["brand_tag"]= info.isMember("brand_tag") ? info["brand_tag"].asString():"";
        }
        else
        {
            string strbrand = m_hotelData[i]["brand"].asString();
            if(!strbrand.empty())
            {
                brand = strbrand[0];
            }
            property3.assign(5,0);
            if(brand == '4') property3[3] = 1;
            else if(brand == '3') property3[2] = 1;
            else if(brand == '2') property3[1] = 1;
            else if(brand == '1') property3[0] = 1;
            else property3[4] = 1;
            m_hotelData[i]["brand_tag"]= strbrand;
        }
         //属性1: 星级
        /*int star = info.isMember("star") ? boost::lexical_cast<int>(info["star"].asString()) : 1;
        if(star<1 or star>5) star=1;//1星级为最低等级,高于5星级为异常情况
        //青年旅行社最高被视为2星级酒店
        if(m_hotel_map[uid]["hotel_type"].asString()=="1" and star>2) star=2;
        hotels[i]["as_star"] = star;*/

        int star = m_hotelData[i]["star"].asInt();
        //int star = boost::lexical_cast<int>(m_hotelData[i]["star"].asDouble());
        if(star < 1 or star > 5) star = 1;
        //青年旅行社最高被视为2星级酒店
        if(!m_isPrivate and m_hotel_map[uid]["hotel_type"].asString() == "1" and star > 2) star = 2;
        m_hotelData[i]["as_star"] = star;
        info["star"] = star;
        double star_ratio=1;
        if (prefer_stars.find(star)==prefer_stars.end() or (single_select_4 and brand=='1'))
        {
            star_ratio=0.2;// single_select_4 and brand=='1' 时 为此值
            if(star<*prefer_stars.begin())
            {
                int star_diff=*prefer_stars.begin()-star;
                if(star_diff>0 and star_diff<=4) star_ratio*=1-0.2*star_diff;
            }
            else if(star>*prefer_stars.rbegin())
            {
                int star_diff=star-*prefer_stars.rbegin();
                if(star_diff>0 and star_diff<=4) star_ratio*=1-0.2*star_diff;
            }
        }
        prefer_ratio*=star_ratio;
        cur_ticket["prefer_ratios"]["star_ratio"]=star_ratio;
        property1.assign(5, 0);
        property1[5-star]=1;

        //价格不可信则予以惩罚
        double price_confidence_ratio=1;
        string type=star>2?boost::lexical_cast<string>(star):"13";
        string city_star_statis_price_key=m_cid+"_"+type;
        if(m_city_hotel_statis_price_map.find(city_star_statis_price_key)!=m_city_hotel_statis_price_map.end())
        {
            double statis_price=m_city_hotel_statis_price_map[city_star_statis_price_key]*0.8;
            double ticket_price=m_hotelData[i]["room"][0u]["price"].asInt();
            if(ticket_price<statis_price) price_confidence_ratio=pow(ticket_price/statis_price,5);
        }
        prefer_ratio*=price_confidence_ratio;
        cur_ticket["prefer_ratios"]["price_confidence_ratio"]=price_confidence_ratio;


        //属性2:评价得分
        double grade_order= (hotel_grade_order.count(uid) == 1) ? hotel_grade_order[uid] : 1;
        //double grade = m_hotel_map[uid].isMember("grade") ? boost::lexical_cast<double>(m_hotel_map[uid]["grade"].asString()) : 0;
        string gradeStr = m_hotelData[i]["score"].asString();
        double grade = boost::lexical_cast<double>(gradeStr);
        property2[0] = 1-grade_order;
        double grade_ratio=1;
        if (grade_order-0.3>eps or grade-7.2<-eps) grade_ratio=0.5;
        quality_ratio*=grade_ratio;
        cur_ticket["quality_ratios"]["grade_ratio"]=grade_ratio;

        //属性4: 离市中心的距离
        double dis_order = (hotel_dis_order.count(uid) == 1) ? hotel_dis_order[uid] : 1;
        double dis_score = 0;
        if (dis_order < 0.2) dis_score = 1;
        else dis_score = 1.25 -1.25* dis_order;
        property4[0] = dis_score;
        double dis_ratio=1;
        if (dis_order>0.5) dis_ratio=0.5;
        quality_ratio*=dis_ratio;
        cur_ticket["quality_ratios"]["dis_ratio"]=dis_ratio;

        double img_ratio=1;
        if(!m_isPrivate and m_hotel_map[uid]["no_img"].asString()=="1") img_ratio=0.2;
        quality_ratio*=img_ratio;
        cur_ticket["quality_ratios"]["img_ratio"]=img_ratio;

        propertyM.clear();
        propertyM.push_back(property1);
        propertyM.push_back(property2);
        propertyM.push_back(property3);
        propertyM.push_back(property4);
        vector<double> sub_scores;
        m_hotelData[i]["qualityScore"] = quality_ratio*CalScore(sub_scores, m_proportionV, m_qualityM, propertyM);
        m_hotelData[i]["preferScore"] = m_hotelData[i]["qualityScore"].asDouble()*prefer_ratio;

        cur_ticket["info"] = info;
        cur_ticket["dis_order"] = dis_order ;
        cur_ticket["grade_order"] = grade_order;
        cur_ticket["qualityScore"] = m_hotelData[i]["qualityScore"];
        cur_ticket["preferScore"] = m_hotelData[i]["preferScore"];
        cur_ticket["sub_scores"] = Vector2Json(sub_scores);
        cur_ticket["prefer_ratio"] = prefer_ratio;
        cur_ticket["quality_ratio"] = quality_ratio;
        cur_ticket["price"] = m_hotelData[i]["room"][0u]["price"];
        m_debug_res[uid] = cur_ticket;
    }

    return true;
}

bool Hotel::GetScoresParallel(Json::Value &data)
{
    int thread_num = data.size();
    MyThreadPool *myThreadPool = new MyThreadPool;
    int threadStackSize = 102400;
    if(thread_num <= 0 || myThreadPool->open(thread_num,threadStackSize)!=0){
        delete myThreadPool;
        _ERROR("MyThreadPool open failed!,thread_num:%d",thread_num);
        return false;
    }

    myThreadPool->activate();
    std::vector<MJ::Worker*> jobs;
    for(int i = 0; i < data.size(); i++)
    {
        Hotel *hotel = new Hotel(*this);
        hotel->m_hotelData=data[i]["hotel"];
        hotel->m_isPrivate=(data[i]["private"].asInt() == 1?true:false);
        hotel->m_cid = data[i]["cid"].asString();
        jobs.push_back((MJ::Worker*)hotel);
        myThreadPool->add_worker((MJ::Worker*)hotel);
    }

    myThreadPool->wait_worker_done(jobs);

    Json::Value mergedResult;
    //hotelList 是保序的
    for(int i = 0; i < jobs.size(); ++i){
        Hotel *hotel = (Hotel*)jobs[i];
        MergeResult(hotel,mergedResult);
        delete hotel;
    }

    delete myThreadPool;
    m_hotelData = mergedResult;

    return true;
}

void Hotel::MergeResult(Hotel *hotel,Json::Value &mergedResult)
{
    Json::Value debug = hotel->m_debug_res;
    vector<string> hotel_keys = debug.getMemberNames();
    for(int i = 0;i < hotel_keys.size(); i++){
        m_debug_res[hotel_keys[i]] = debug[hotel_keys[i]];
    }

    if(hotel->m_hotelData.size()==0)
    {
        mergedResult.append(Json::Value());
        return;
    }
    Json::Value tmp;
    tmp["private"] = hotel->m_isPrivate ? 1 : 0;
    tmp["cid"] = hotel->m_cid;
    tmp["hotel"] = hotel->m_hotelData;
    mergedResult.append(tmp);
}
