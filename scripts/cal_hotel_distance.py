# -*- coding: utf-8 -*-
import sys
import MySQLdb
reload(sys)
sys.setdefaultencoding("utf-8")
import math

def get_hotels():
    conn = MySQLdb.connect(host='10.10.111.62', port=3306, user='reader', passwd='miaoji1109', db='base_data')
    cur = conn.cursor()
    sql = 'select city_mid, uid, map_info from hotel'
    cur.execute(sql)

    hotels = {}
    for x in cur.fetchall():
        hotels.setdefault(x[0], []).append(x[1:])
    cur.close()
    conn.close()
    return hotels

EARTH_RADIUS = 6378137.

LIMIT = math.pow(math.sin(1000./ (EARTH_RADIUS * 2)), 2)

def rad(d):
     return d * math.pi / 180.0


def is_1km(latlng1, latlng2):
    lat1, lng1 = latlng1.split(',')
    lat2, lng2 = latlng2.split(',')
    lat1, lng1, lat2, lng2 = float(lat1), float(lng1), float(lat2), float(lng2)
    radLat1 = rad(lat1)
    radLat2 = rad(lat2)
    a = radLat1 - radLat2
    b = rad(lng1) - rad(lng2)
    s = math.pow(math.sin(a/2),2) + math.cos(radLat1) * math.cos(radLat2) * math.pow(math.sin(b/2), 2)
    return s < LIMIT


def execute_sqls(sql, tups):
    conn = MySQLdb.connect(host='127.0.0.1', user='root', passwd='root', db='ppscore')
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


def insert(name, table, tups):
    if isinstance(name, str):
        name = ['`' + x.strip() + '`' for x in name.split(',')]
    t = table, ','.join(name), ','.join(['%s'] * len(name))
    sql = 'INSERT INTO %s (%s) VALUES(%s)' % t
    return execute_sqls(sql, tups)


if __name__ == '__main__':
    hotels_dic = get_hotels()
    name = 'uid, count_1km'
    print "city num: %s" % len(hotels_dic)
    for mid, hotels in hotels_dic.items():
        print "deal city:", mid
        n = len(hotels)
        print "hotel num: %s" % n
        tups = []
        distances = [[False] * n for x in range(n)]
        for i in range(n):
            print i
            try:
                for j in range(n):
                    distances[i][j] = distances[j][i] if j <= i else is_1km(hotels[i][1], hotels[j][1])
                t = hotels[i][0], sum(distances[i])
            except Exception, e:
                print e
            else:
                tups.append(t)
        print "insert:", insert(name, 'hotel_distance', tups)
    print "down!"


