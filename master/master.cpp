#include "master.hpp"

Master *Master::m_pInstance = NULL;


Master::Master()
{
    assert(Init());
}


bool Master::Init()
{
    if(!Traffic::Init())
    {
        _ERROR("Init traffic  error!");
        return false;
    }
    if(!Hotel::Init())
    {
        _ERROR("Init hotel  error!");
        return false;
    }

    return true;
}

string Master::ppScore(const string &query, const string &type,const string &qid, const string & uid, const string & csuid, string & other_params,string ptid)
{
    _INFO("begin");
    _INFO("%s,%s,%s,%s", type.c_str(), qid.c_str(), uid.c_str(), csuid.c_str());
    string res;
    if (type == "csv010"){
        Hotel hotel;
        hotel.m_qid=qid;
        hotel.m_uid=uid;
        hotel.m_csuid=csuid;
        res = hotel.csv010(query,qid,other_params,ptid);
    }
    else if(type == "csv020"){
        Hotel hotel;
        hotel.m_qid = qid;
        hotel.m_uid = uid;
        hotel.m_csuid = csuid;
        res = hotel.csv020(query,qid,other_params,ptid);
    }
    else if (type == "csv011"){
        Json::Value jValue;
        Traffic traffic(jValue);
        traffic.m_qid=qid;
        traffic.m_uid=uid;
        traffic.m_csuid=csuid;
        traffic.csv011(query,qid,other_params,res);
    }
    else if (type == "csv012ScoreAnalyse"){
        Json::Value jValue;
        Traffic traffic(jValue);
        traffic.m_qid=qid;
        traffic.m_uid=uid;
        traffic.m_csuid=csuid;
        res = traffic.csv012ScoreAnalyse(query,qid,other_params);
    }
    else
    {
        res = "{\"error\":{\"error_id\":101,\"error_str\":\"type is not support\"}}";
    }
    _INFO("end");
    return res;
}

