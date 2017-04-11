#include "CsvCommon.hpp"

double rad(double d)
{
  return d * pi /180.0;
}


int GetDistance(double lat1, double lng1, double lat2, double lng2)
{
  double radLat1;
  double radLat2;
  double a;
  double b;

  radLat1 = rad(lat1);
  radLat2 = rad(lat2);
  a = radLat1 - radLat2;
  b = rad(lng1) - rad(lng2);

  double s = 2 * asin(sqrt(pow(sin(a/2),2) + cos(radLat1)*cos(radLat2)*pow(sin(b/2),2)));
  return s * EARTH_RADIUS;
}


Json::Value Vector2Json(const vector<double> &v){
    Json::Value res;
    for (vector<double>::const_iterator it = v.begin(); it != v.end(); ++it){
        res.append(*it);
    }
    return res;
}

Json::Value Vector2Json(const vector<vector<double>>& v){
    Json::Value res;
    for (vector<vector<double>>::const_iterator it = v.begin(); it != v.end(); ++it)
    {
        Json::Value _res;
        for (vector<double>::const_iterator _it = it->begin(); _it != it->end(); ++_it)
            _res.append(*_it);
        res.append(_res);
    }
    return res;
}


Json::Value Vector2Json(const vector<int>& v){
    Json::Value res;
    for (vector<int>::const_iterator it = v.begin(); it != v.end(); ++it){
        res.append(*it);
    }
    return res;
}

//打印向量
int cout_matrix(const vector<double> &v){
    for (int i = 0; i < v.size(); i++)
        cout << v[i] << " ";
    cout << endl;
}

//打印矩阵
int cout_matrix(const vector<vector<double>> &v){
    for (int i = 0; i < v.size(); i++){
        cout_matrix(v[i]);
    }
    cout << "/";
}

//属性的 归一化: 第一个参数是下标值
//<2, 3>  & <4, 3, 2, 1>        -->    <0, 0, 0.5, 0.5>
//<1, 3, 4 ,4>  & <4, 3, 2, 1>  -->    <0, 0.5, 0, 0.5> 最后的两个“4”溢出
vector<double> normalization(vector<int> &v, vector<double> &quality){
    vector<double> res;
    int n = quality.size();

    int v_len = 0;
    for(int i=0; i < v.size(); i++)
    {
        if(v[i]<=n-1 and v[i]>=0) v_len++;
    }

    for (int i = 0; i < n; i++){
        res.push_back(std::count(v.begin(), v.end(), i) / (double) v_len);
    }
    return res;
}

//向量叉乘
vector<double> vector_product(const vector<double> &v1, const vector<double> &v2){
    assert (!v1.empty() && !v2.empty() && v1.size() == v2.size());
    vector<double> res;
    for (int i = 0; i < v1.size(); i++){
        res.push_back(v1[i] * v2[i]);
    }
    return res;
}

//向量点乘
double vector_product_dot(const vector<double> &v1, const vector<double> &v2){
    assert (!v1.empty() && !v2.empty() && v1.size() == v2.size());
    int n = v1.size();
    double sum = 0;
    for (int i = 0; i < n; i++){
        sum += (v1[i] * v2[i]);
    }
    return sum;
}

//计算得分, res 记录子得分
double CalScore(vector <double> &scores,
        const vector<double> &proportionV,
        const vector<vector<double>> &qualityM,
        const vector<vector<double>> &propertyM,double score_total)
{
    scores.clear();
    double total = accumulate(proportionV.begin(), proportionV.end(), 0.0);
    int n = proportionV.size();
    for (int i = 0; i< n; i++){
        double t1 = vector_product_dot(qualityM[i], propertyM[i]) / qualityM[i][0];
        double t2 = proportionV[i] * score_total/ total;
        scores.push_back(t1 * t2);
    }
    return  accumulate(scores.begin(), scores.end(), 0.0);
}

vector<string> StrSplit(const string& str, const string& pattern)
{
    vector<string> v;
    size_t bpos = 0;

    while(str.find(pattern, bpos) != std::string::npos)
    {
        size_t epos = str.find(pattern, bpos);
        if(epos == 0)
        {
            bpos = epos + pattern.size();
            continue;
        }
        v.push_back(str.substr(bpos, epos - bpos));
        bpos = epos + pattern.size();
        if(bpos >= str.size())
            break;
    }

    if(bpos < str.size())
        v.push_back(str.substr(bpos, str.size() - bpos));

    return v;
}

string StrJoin(const vector<string> &v, const string &str){
    string res;
    int n = v.size();
    for(int i = 0; i < n; i++){
        res += (i == n - 1 ? v.at(i) : (v.at(i) + str));
    }
    return res;
}

int logException(const char* type,
        const char* uid,
        const char* csuid,
        const char* qid,
        const char* querystring
        ){
    time_t __time_buf__;
    tm __localtime_buf__;
    char __strftime_buf__[18];
    time(&__time_buf__);
    localtime_r(&__time_buf__, &__localtime_buf__);
    strftime(__strftime_buf__, 18, "%Y%m%d %H:%M:%S", &__localtime_buf__);
    struct timeval tm;
    gettimeofday(&tm, NULL);
    unsigned long ms = tm.tv_sec * 1000 + tm.tv_usec / 1000;
    fprintf(stderr, "[%s][Exception MiojiOPObserver,type=%s,uid=%s,csuid=%s,qid=%s,ts=%ld,querystring=%s]\n",__strftime_buf__,type, uid, csuid, qid, ms, MJ::UrlEncode(querystring).c_str());
    return 0;
}

//并行化程序后，日志信息转换为字符串类型并输出
void logInfo2Str(map<string,string>& log_info)
{
    string res = "";
    for(auto it = log_info.begin(); it != log_info.end(); ++it)
    {
        res += (*it).first;
        res += "=";
        res += (*it).second;
        res += ",";
    }

    _INFO("%s %s","[LOGINFO]",res.c_str());
}
