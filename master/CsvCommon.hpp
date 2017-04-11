#ifndef _CSVCOMMON_HPP_
#define _CSVCOMMON_HPP_

#include "MJCommon.h"
#include <assert.h>
#include <sstream>
#include <algorithm>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/timer.hpp>
#include <map>
#include <math.h>
#include "json/json.h"
#include <numeric>
#include <list>
#include <ext/vstring.h>
#include <ext/vstring_fwd.h>
using namespace std;

#define pi 3.1415926535897932384626433832795
#define eps 0.000001

int GetDistance(double lat1, double lng1, double lat2, double lng2);

int cout_matrix(const vector<vector<double>> &v);
int cout_matrix(const vector<double> &v);

Json::Value Vector2Json(const vector<double> &v);
Json::Value Vector2Json(const vector<int> &v);
Json::Value Vector2Json(const vector<vector<double>> &v);

vector<double> normalization(vector<int> &v, vector<double> &quality);
vector<double> vector_product(const vector<double> &v1, const vector<double> &v2);
double vector_product_dot(const vector<double> &v1, const vector<double> &v2);

double CalScore(vector<double> &res,
        const vector<double> &proportionV,
        const vector<vector<double>> &qualityM,
        const vector<vector<double>> &propertyM,double score_total=10.0);

vector<string> StrSplit(const string& str, const string& pattern);
string StrJoin(const vector<string> &v, const string &str);

int logException(const char* type,
        const char* uid,
        const char* csuid,
        const char* qid,
        const char* querystring
        );
void logInfo2Str(map<string,string>&);
#endif
