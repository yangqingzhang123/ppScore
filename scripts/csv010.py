#!/usr/bin/python
#coding: utf-8

import urllib
import urllib2
import time
import json
from DBHandle import DBHandle

def main():
    citys=['10009','10040','50005','50004','20150','20152','20043','20139','20083','20186','50001','30042','60001']
    domain='http://10.10.135.140:91/?qid=1234&type=csv010&query='
    #domain='http://127.0.0.1:8899/?qid=1234type=csv010&query='
    params='{"prefer":{"hotel":{"roomPrefer":{"adults":2,"child":0},"type":["15","8"]} },"checkin":"20160909","checkout":"20160912","cid":"10001","occ":2,"budget":1000}'

    city_hotel={}
    htids=[]
    hotel_scores={}
    for i in range(len(citys)):
        params_d=json.loads(params)
        params_d['cid']=citys[i]
        params=json.dumps(params_d)
        url=domain+urllib.quote(params)
        #print url
        start=time.time()
        result=urllib2.urlopen(urllib2.Request(url)).read()
        end=time.time()
        during=end-start
       #print 'hsv003 cost',during
        try:
            final_result=json.loads(result)
        except Exception,e:
                print Exception,":",e,citys[i]
                continue
        print len(final_result["data"]["hotel"])
        print final_result
        hotels=final_result['data']['hotel']
        hotels_sorted=sorted(hotels,key=lambda x:x['qualityScore'],reverse=True)[0:10]
        htids_=[x['id'] for x in hotels_sorted]
        city_hotel[citys[i]]=htids_
        htids+=htids_
        for hotel in hotels_sorted:
            hotel_scores[hotel['id']]=hotel['qualityScore']


    #print hotel_scores
    #print city_hotel
    #print htids

    onlinedb=DBHandle("10.10.111.62","reader","miaoji1109","base_data")
    sql="SELECT uid,hotel_name,hotel_name_en,city,city_mid,map_info,star,grade,brand_tag FROM `hotel` where uid in ('%s')"%"','".join(htids)
    #print sql
    results=onlinedb.do(sql)
    print ",".join(["id","中文名","英文名","所属城市","所属城市id","位置","星级","评分","品牌","品质得分"])
    for record in results:
        print ",".join([record['uid'],record['hotel_name'],record['hotel_name_en'].replace(",",";"),record['city'],record['city_mid'],str(record['map_info'].replace(",",";")),str(record['star']),str(record['grade']),record['brand_tag'],str(hotel_scores[record['uid']])]).encode('utf8')

if __name__ == '__main__':
    main()
