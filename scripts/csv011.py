# -*- coding: utf8 -*-
'''
用于评价ppscore交通接口
'''
import csv
import requests
import json
from pprint import pprint
from datetime import datetime
from datetime import timedelta
import sys
reload(sys)
sys.setdefaultencoding('utf-8')
import MySQLdb

conn = MySQLdb.connect(host='10.10.111.62', user='reader', passwd='miaoji1109', db='base_data', charset='utf8')
# conn = MySQLdb.connect(host='127.0.0.1', user='root', passwd='platform', db='hoteldb2')
cur = conn.cursor()

sql = "select id from city where country = '中国'"
cur.execute(sql)
China = set(cur.fetchall())
assert China, 'china none'

sql = "select id, name, time_zone from city"
cur.execute(sql)
tmp = list(cur.fetchall())
TimeZone = dict((str(x[0]), float(x[2])) for x in tmp)
CityName = dict((x[0], x[1]) for x in tmp)
CityId = dict((x[1], str(x[0])) for x in tmp)
assert TimeZone and CityName, 'TimeZone or CityName None'

sql = 'select code, star from airlines'
cur.execute(sql)
Airlines = dict((x[0], x[1]) for x in cur.fetchall())
assert Airlines, 'Airlines None'

sql = 'select id, seat_type_en from norm_train_seat'
cur.execute(sql)
TrainSeat = dict((str(x[0]), x[1]) for x in cur.fetchall())

cur.close()
conn.close()

Cabin = [u'first class',u'business',u'super economy',u'economy',u'cheap air'][::-1]


# 坏时间：国外起飞坏时间：01：00-07：00，国外降落坏时间：00：00-05：00，国内起飞坏时间：03：00-07：00，国内降落坏时间：00：00-05：00
def is_bad_time(t, city, dept):
    t = datetime.strptime(t, '%H:%M')
    ts = [datetime.strptime(x, '%H:%M') for x in ['00:00', '01:00', '03:00', '05:00', '07:00']]
    if city not in China:
        return ['good', 'bad'][ts[1] < t < ts[-1] if not dept else ts[0] < t < ts[3]]
    else:
        return ['good', 'bad'][ts[2] < t < ts[-1] if not dept else ts[0] < t < ts[3]]

def total_seconds(dur):
    return dur.microseconds / 10 ** 6+ (dur.seconds + dur.days * 24. * 3600)

def cal_dur(ts1, ts2, city1, city2):
    print('\n')
    print(ts1)
    print(ts2)
    t1 = datetime.strptime(ts1, '%Y%m%d_%H:%M')
    t2 = datetime.strptime(ts2, '%Y%m%d_%H:%M')
    dur = t2 - t1
    print(dur)
    dur = total_seconds(dur) / 60. /60.
    print(dur)
    print(city1)
    print(city2)
    print("city1: %s" % TimeZone[city1])
    print("city2: %s" % TimeZone[city2])
    tmp = dur - TimeZone[city1] + TimeZone[city2]
    print(tmp)
    print("\n")
    return ts1, ts2, dur - TimeZone[city2] + TimeZone[city1]

# 价格|MD5|后台交通得分|来源数目|来源;
# 出发时间|到达时间|出发机场|目的机场|航空公司ID|仓位ID|机型ID;
# 出发时间|到达时间|出发机场|目的机场|航空公司ID|仓位ID|机型ID”
def parse_flight(strs):
    type = 'flight'
    t = [x.split('|') for x in strs.split(';')]
    score = t[0][2]
    stop = len(t) - 2
    dur = cal_dur(t[1][0], t[-1][1], t[1][2].split('#')[1], t[-1][3].split('#')[1])
    dept_dest_t = [[(x[0][-5:], is_bad_time(x[0][-5:], x[2].split('#')[1], 0)),
                    (x[1][-5:], is_bad_time(x[1][-5:], x[3].split('#')[1], 1))]
                   for x in t[1:]]

    airline = [(x[4], Airlines.get(x[4])) for x in t[1:]]
    cabin = [Cabin[int(x[5])].encode('utf-8') for x in t[1:]]
    dept_city, dest_city = t[1][2].split('#')[1], t[-1][3].split('#')[1]
    dept_city, dest_city = CityName.get(dept_city, dept_city), CityName.get(dest_city, dest_city)
    return dept_city, dest_city, type, score, cabin, stop, dur, dept_dest_t, airline, strs


# 价格|MD5|后台交通得分|来源数目|来源;出发时间|到达时间|出发车站|目的车站|仓位ID;出发时间|到达时间|出发车站|目的车站|仓位ID
def parse_train(strs):
    type = 'train'
    t = [x.split('|') for x in strs.split(';')]
    score = t[0][2]
    stop = len(t) - 2
    dur = cal_dur(t[1][0], t[-1][1], t[1][2].split('#')[1], t[-1][3].split('#')[1])
    cabin = [TrainSeat.get(x[-1]) for x in t[1:]]
    dept_dest_t = [[(x[0][-5:], is_bad_time(x[0][-5:], x[2].split('#')[1], 0)),
                    (x[1][-5:], is_bad_time(x[1][-5:], x[3].split('#')[1], 1))]
                   for x in t[1:]]
    airline = None
    dept_city, dest_city = t[1][2].split('#')[1], t[-1][3].split('#')[1]
    dept_city, dest_city = CityName.get(dept_city, dept_city), CityName.get(dest_city, dest_city)
    return dept_city, dest_city, type, score, cabin, stop, dur, dept_dest_t, airline, strs


# 价格|MD5|后台交通得分|来源数目|来源;出发时间|到达时间|出发车站|目的车站;出发时间|到达时间|出发车站|目的车站
def parse_bus(strs):
    type = 'bus'
    t = [x.split('|') for x in strs.split(';')]
    score = t[0][2]
    stop = len(t) - 2
    dur = cal_dur(t[1][0], t[-1][1], t[1][2].split('#')[1], t[-1][3].split('#')[1])
    cabin = airline = None
    dept_dest_t = [[(x[0][-5:], is_bad_time(x[0][-5:], x[2].split('#')[1], 0)),
                    (x[1][-5:], is_bad_time(x[1][-5:], x[3].split('#')[1], 1))]
                   for x in t[1:]]
    dept_city, dest_city = t[1][2].split('#')[1], t[-1][3].split('#')[1]
    dept_city, dest_city = CityName.get(dept_city, dept_city), CityName.get(dest_city, dest_city)
    return dept_city, dest_city, type, score, cabin, stop, dur, dept_dest_t, airline, strs


def deal(data, type):
    if not data:
        return []
    return [[parse_flight, parse_train, parse_bus][type](str(x))
            for dept, v in data.items()
            for dest, ticket in v.items()
            for x in ticket]


def eva_traffic(data):
    flight = data.get('flight')
    train = data.get('train')
    bus = data.get('bus')
    combine = data.get('combine')
    roundTrip = data.get('roundTrip')
    InterLine = data.get('InterLine')

    all_tickets = []
    all_tickets.extend(deal(flight, 0))
    all_tickets.extend(deal(train, 1))
    all_tickets.extend(deal(bus, 2))
    all_tickets = sorted(all_tickets, key=lambda x:float(x[3]), reverse=True)

    with open('result.csv', 'wb') as csvfile:
        spamwriter = csv.writer(csvfile, delimiter=',')
        t = "出发城市", '到达城市', '票类型', '得分', '各段舱位', '中转次数', '总耗时', '各段出发到达时间', '各段航空公司', '原始票信息'
        spamwriter.writerow(t)
        for x in all_tickets:
            spamwriter.writerow(x)
    print 'Down!'


def eva_csv011():
    url = 'http://10.10.135.140:91/'
    #url = 'http://127.0.0.1:6789/trafficsearch?'
    #query = '{"data":"20001|11469#20160911-20160915;20001|10033#20160911-20160915;50006|50012#20160913-20160913;10037|10046#20160919-20160925;10048|10037#20160918-20160924;10046|10008#20160922-20160929;20003|10013#20160911-20160912;10014|10048#20160916-20160922;10008|20003#20160928-20160930;10013|10014#20160914-20160921","dataHeadTail":"20003-10013:10008-20003","dataRound":"20003:10013","data_round":"20003:10013","endDate":"20160931","end_date":"1472594400","end_date_s":"20160931_06:00","interval":60,"intervalNum":1,"interval_num":1,"prefer":{"flight":{"black":["MH"],"class":[1],"com":[],"time":[],"type":[1,2,3]},"global":{"mode":["flight", "train"],"prefer":0,"transit":3},"train":{"class":[1],"time":[]}},"startDate":"20160911","start_date":"1470909600","start_date_s":"20160911_18:00"}'
    query = '{"data":"","endDate":"20160931","end_date":"1472594400","end_date_s":"20160931_06:00","interval":60,"intervalNum":1,"interval_num":1,"prefer":{"flight":{"black":["MH"],"class":[1],"com":[],"time":[],"type":[1,2,3]},"global":{"mode":["flight", "train"],"prefer":0,"transit":3},"train":{"class":[1],"time":[]}},"startDate":"20160921","start_date":"1470909600","start_date_s":"20160921_18:00"}'
    cityPairs=["北京-旧金山","广州-墨尔本","上海-大阪","阿姆斯特丹-北京","卡尔加里-多伦多","巴塞罗那-伦敦","纽约-拉斯维加斯","伊斯坦布尔-雅典","法兰克福-苏黎世","布拉格-维也纳"]
    oneWay=';'.join(list('|'.join((CityId[x.strip().split('-')[0].strip().decode('utf8')],CityId[x.strip().split('-')[1].strip().decode('utf8')]+"#20160921-20160929")) for x in cityPairs) )
    tmp=json.loads(query)
    tmp['data']=oneWay
    query=json.dumps(tmp)
    print query
    r = requests.get(url=url, params=dict(query=query, type='csv011',qid='1234'))
    #print r.content
    data = json.loads(r.content).get('data')
    eva_traffic(data)

eva_csv011()
# eva_traffic(json.loads(data).get('data'))
