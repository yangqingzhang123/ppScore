#!/usr/bin/python
#coding: utf-8

import urllib
import urllib2
import time
import json

def main():

    domain='http://10.10.135.140:91/?type=csv012&qid=881206&tid=&lang=zh_cn&ccy=CNY&query='
    #domain='http://10.10.165.125:6789/?type=csv012&qid=1206&tid=&lang=zh_cn&ccy=CNY&query='
    #domain='http://10.10.135.140:91/?type=tsv006&qid=default2&tid=&lang=zh_cn&ccy=CNY&query='
    params='{"info":[{"deptCity":["10048"],"destCity":["10001"],"mode":["flight","train","bus"],"trafficRange":[{"fromEarly":"20160828_02:00","fromLate":"20160828_18:00","toEarly":"20160828_04:00","toLate":"20160828_22:00"}]}],"prefer":{"flight":{"black":[],"class":[],"com":[],"time":[],"type":[]},"global":{"mode":["flight"],"prefer":0,"transit":1},"train":{"class":[],"time":[]}}}'
    params='{\"info\":[{\"deptCity\":[\"20003\"],\"destCity\":[\"10009\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161016_08:30\",\"fromEarly\":\"20161016_08:30\",\"fromLate\":\"20161019_08:30\",\"to\":\"20161019_19:40\",\"toEarly\":\"20161016_19:40\",\"toLate\":\"20161019_19:40\"}]},{\"deptCity\":[\"10002\"],\"destCity\":[\"10054\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161019_02:30\",\"fromEarly\":\"20161019_02:30\",\"fromLate\":\"20161022_02:30\",\"to\":\"20161022_03:25\",\"toEarly\":\"20161019_03:25\",\"toLate\":\"20161022_03:25\"}]},{\"deptCity\":[\"10054\"],\"destCity\":[\"10004\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161020_02:05\",\"fromEarly\":\"20161020_02:05\",\"fromLate\":\"20161023_02:05\",\"to\":\"20161023_04:36\",\"toEarly\":\"20161020_04:36\",\"toLate\":\"20161023_04:36\"}]},{\"deptCity\":[\"10004\"],\"destCity\":[\"10018\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161022_04:48\",\"fromEarly\":\"20161022_04:48\",\"fromLate\":\"20161025_04:48\",\"to\":\"20161025_05:55\",\"toEarly\":\"20161022_05:55\",\"toLate\":\"20161025_05:55\"}]},{\"deptCity\":[\"10018\"],\"destCity\":[\"10102\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161024_00:32\",\"fromEarly\":\"20161024_00:32\",\"fromLate\":\"20161027_00:32\",\"to\":\"20161027_02:35\",\"toEarly\":\"20161024_02:35\",\"toLate\":\"20161027_02:35\"}]},{\"deptCity\":[\"10102\"],\"destCity\":[\"10003\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161025_05:10\",\"fromEarly\":\"20161025_05:10\",\"fromLate\":\"20161028_05:10\",\"to\":\"20161028_06:35\",\"toEarly\":\"20161025_06:35\",\"toLate\":\"20161028_06:35\"}]},{\"deptCity\":[\"10003\"],\"destCity\":[\"10005\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161028_02:20\",\"fromEarly\":\"20161028_02:20\",\"fromLate\":\"20161031_02:20\",\"to\":\"20161031_04:55\",\"toEarly\":\"20161028_04:55\",\"toLate\":\"20161031_04:55\"}]},{\"deptCity\":[\"10005\"],\"destCity\":[\"20003\"],\"mode\":[\"train\",\"flight\",\"bus\"],\"trafficRange\":[{\"from\":\"20161030_23:30\",\"fromEarly\":\"20161030_23:30\",\"fromLate\":\"20161102_23:30\",\"to\":\"20161103_17:50\",\"toEarly\":\"20161031_17:50\",\"toLate\":\"20161103_17:50\"}]},{\"deptCity\":[\"20003\",\"10005\"],\"destCity\":[\"10002\",\"20003\"],\"mode\":[\"train\",\"flight\",\"bus\",\"interLine\"],\"trafficRange\":[{\"from\":\"20161016_08:30\",\"fromEarly\":\"20161016_08:30\",\"fromLate\":\"20161019_08:30\",\"to\":\"20161019_19:40\",\"toEarly\":\"20161016_19:40\",\"toLate\":\"20161019_19:40\"},{\"from\":\"20161030_23:30\",\"fromEarly\":\"20161030_23:30\",\"fromLate\":\"20161102_23:30\",\"to\":\"20161103_17:50\",\"toEarly\":\"20161031_17:50\",\"toLate\":\"20161103_17:50\"}]}],\"prefer\":{\"flight\":{\"black\":[],\"class\":[],\"com\":[],\"time\":[],\"type\":[]},\"global\":{\"mode\":[\"flight\"],\"prefer\":0,\"transit\":1},\"train\":{\"class\":[],\"time\":[]}}}'

    url=domain+urllib.quote(params)
    print url
    start=time.time()
    result=urllib2.urlopen(urllib2.Request(url)).read()
    print result
    final_result=json.loads(result)
    end=time.time()
    during=end-start
    print 'csv012 cost',during
    print result

if __name__ == '__main__':
    main()
