# -*- coding: utf-8 -*-
"""
读取数据库,统计成功联率
每天
"""
from Email import Email
import MySQLdb
from datetime import datetime
from datetime import timedelta
from prettytable import PrettyTable
import sys
reload(sys)
sys.setdefaultencoding('utf8')


def get_data():
    conn = MySQLdb.connect(host='10.10.151.68', user='root', passwd='miaoji@2014!', db='monitor')
    cur = conn.cursor()

    yesterday = (datetime.now()-timedelta(days=1)).strftime('%Y%m%d')
    sql = "SELECT rq_type, is_success,link_full, " \
          "is_rq_round, is_rq_interline, round_count, interline_count " \
          "FROM ppscore_monitor where req_time like '%s%%';" % yesterday

    cur.execute(sql)
    d = {}
    for x in cur.fetchall():
        d.setdefault(x[0], []).append(map(bool, x[1:]))

    cur.close()
    conn.close()
    return d

if __name__ == '__main__':
    email = Email('csv010&csv011&csv012统计', ['lidongwei@mioji.com', 'zhangyang@mioji.com', 'huxuanzheng@mioji.com','wangfang@mioji.com'])
    d = get_data()
    msg = PrettyTable(['类型','总数','成功','全联通','往返请求','联程请求'])
    msg.padding_width = 3
    for type_name, v in d.items():
        total = len(v)  # 总共的请求数
        counts = map(sum, zip(*v))  # 各请求的个数
        print counts
        suc,suc_rate=counts[0],counts[0]*1.0/total
        link_full,link_full_rate=counts[1],counts[1]*1.0/total
        tmp=[type_name,total,str(suc)+" (%.2f%%)"%(suc_rate*100),str(link_full)+" (%.2f%%)"%(link_full_rate*100)]
        if type_name=='csv012':
            tmp+=[str(counts[4])+"/"+str(counts[2])+" (%.2f%%)"%(counts[4]*100.0/counts[2]),str(counts[5])+"/"+str(counts[3])+" (%.2f%%)"%(counts[5]*100.0/counts[3])]
            tmp[0]='csv012(交通数据验证)'
        elif type_name=='csv011':
            tmp[4:]=["null"]*2
            tmp[0]='csv011(交通库中数据)'
        elif type_name=='csv010':
            tmp[3:]=["null"]*3
            tmp[0]='csv011(酒店库中数据)'
        msg.add_row(tmp)
    print '\n'
    print msg

    email.addContent(msg.get_string())
    email.launch()
