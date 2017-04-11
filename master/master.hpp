#ifndef _MASTER_HPP_
#define _MASTER_HPP_

#include "Hotel.hpp"
#include "Traffic.hpp"

using namespace std;


class Master{
public:
    static Master* getInstance()
    {
        if(NULL == m_pInstance)
            m_pInstance = new Master();
        return m_pInstance;
    }

    static void release()
    {
        if(NULL != m_pInstance)
        {
            delete m_pInstance;
            m_pInstance = NULL;
        }
    }
    string ppScore(const string &query, const string &type,const string & qid, const string & uid, const string & csuid, string & other_params,string ptid = "");

private:
    Master();
    ~Master();
    Master(const Master&);
    Master &operator=(const Master &rhs);
    static Master *m_pInstance;

    bool Init();
};

#endif
