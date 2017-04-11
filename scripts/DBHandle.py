#!/usr/bin/env python
#coding=UTF-8
'''
	Created on 2016-02-19
	@author: lidw
	@desc:
		数据访问
'''
import MySQLdb
from MySQLdb.cursors import DictCursor

class DBHandle():

	# MySQL 连接信息
	mysql_host = '127.0.0.1'
	mysql_user = 'root'
	mysql_pwd = 'miaoji@2014!'
	mysql_db = 'tmp'

	def __init__(self,host,user,pwd,db):
		self.mysql_host=host
		self.mysql_user=user
		self.mysql_pwd=pwd
		self.mysql_db=db

	def GetConnection(self):
		conn = MySQLdb.connect(host=self.mysql_host, user=self.mysql_user, passwd=self.mysql_pwd, \
							   db=self.mysql_db, charset="utf8")
		return conn

	def ExecuteSQL(self,sql, args = None):
		'''
			执行SQL语句, 正常执行返回影响的行数，出错返回Flase
		'''
		ret = 0
		try:
			conn = self.GetConnection()
			cur = conn.cursor()

			ret = cur.execute(sql, args)
			conn.commit()
		except MySQLdb.Error, e:
			#logger.error("ExecuteSQL error: %s" %str(e))
			return False
		finally:
			cur.close()
			conn.close()

		return ret

	def ExecuteSQLs(self,sql, args = None):
		'''
			执行多条SQL语句, 正常执行返回影响的行数，出错返回Flase
		'''
		ret = 0
		try:
			conn = self.GetConnection()
			cur = conn.cursor()

			ret = cur.executemany(sql, args)
			conn.commit()
		except MySQLdb.Error, e:
			#logger.error("ExecuteSQL error: %s" %str(e))
			return False
		finally:
			cur.close()
			conn.close()

		return ret

	def QueryBySQL(self,sql, args = None):
		'''
			通过sql查询数据库，正常返回查询结果，否则返回None
		'''
		results = []
		try:
			conn = self.GetConnection()
			cur = conn.cursor(cursorclass = DictCursor)

			cur.execute(sql, args)
			rs = cur.fetchall()
			for row in rs :
				results.append(row)
		except MySQLdb.Error, e:
			#logger.error("QueryBySQL error: %s" %str(e))
			print str(e)
			return None
		finally:
			cur.close()
			conn.close()

		return results

#do-->can do almost anything
	def do(self,sql,args = None):
		words=sql.strip().split(' ')
		if words[0].strip().upper()=="SELECT":
			return self.QueryBySQL(sql,args)
		elif (args==None or (not isinstance(args[0],tuple))):
				return self.ExecuteSQL(sql,args)
		else:
				return self.ExecuteSQLs(sql,args)
