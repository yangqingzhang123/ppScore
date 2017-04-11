# -*- coding:utf8 -*-
from datetime import datetime
from datetime import timedelta
import MySQLdb
import re
import heapq
#PATH = '/search/hxz/logs/ppscore/'
PATH = '/search/monitor/dev/ppScore/logs/'
#PATH ='/search/lidw/logs/ppscore/'
#PATH = '/search/yangqingzhang/logs/ppscore/20170113/20170113_11.log'
patt = r'\[(.*)\]  \[(.*)\] \[(.*)\] \[(.*)\] (.*)'
TYPE = ['roundTrip', 'InterLine']
NAME = 'req_time, qid, rq_type, rq_consume, process_consume, ' \
       'is_rq_round, is_rq_interline, round_count, round_price, interline_count, interline_price, ' \
       'is_success, link_full'


def execute_sqls(sql, tups):
    conn = MySQLdb.connect(host='10.10.151.68', user='root', passwd='miaoji@2014!', db='monitor')
    cur = conn.cursor()
    try:
        ret = cur.executemany(sql, tups)
        conn.commit()
    except MySQLdb.Error, e:
        print e
        return 0
    else:
        return ret
    finally:
        cur.close()
        conn.close()


def insert_db(name, table, tups):
    if isinstance(name, str):
        name = ['`' + x.strip() + '`' for x in name.split(',')]
    t = table, ','.join(name), ','.join(['%s'] * len(name))
    sql = 'INSERT IGNORE INTO %s (%s) VALUES(%s)' % t
    return execute_sqls(sql, tups)


def cal_count_price(tickets):
    if not tickets:
        return 0, 0
    tickets = map(float, tickets)
    low10 = heapq.nsmallest(10, tickets)
    return len(tickets),  sum(low10) * 1.0 / len(low10)


def build_t(req_time,req_info):
    t1 = [req_time]
    list_info = req_info.split(',')
    dic = {}
    for li in list_info:
        key_value = li.split('=')

        if len(key_value) == 2:
            dic[key_value[0]] = key_value[1]


    items = NAME.split(', ');
    for i in range(1,len(items)):
        t1.append(dic.get(items[i],"0"))

    return t1


def monitor_log():
    """
    统计上一个小时的log
    :return:
    """
    cur_h = (datetime.now() - timedelta(hours=1)).strftime('%Y%m%d_%H')
    cur_file = PATH + cur_h[:8] + '/' + cur_h + ".log"
    tups = []
    dic = {}
    with open(cur_file, 'r') as f:
        for line in f.readlines():
            line = re.findall(patt, line)
            if(len(line) > 0):
                line = line[0]
            else:
                continue
            if len(line) == 5 and line[3] == "LOGINFO":
                req_time = line[1]
                req_info = line[4]
                t = build_t(req_time,req_info)
                tups += [t]
    print 'insert:', insert_db(NAME, 'ppscore_monitor', tups)

if __name__ == '__main__':
    monitor_log()

