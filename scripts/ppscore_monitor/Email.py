#!/usr/bin/python
# -*- coding:UTF-8 -*-

import os
import smtplib  
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText


class Email:
    def __init__(self, subject, receiver, sender = 'monitor<monitor@mioji.com>'):
        self.msg = MIMEMultipart()
        self.msg['Subject'] = subject
        self.msg['To'] = ';'.join(receiver)
        self.msg['From'] = sender
        pass

    def __del__(self):
        pass

    def addContent(self, data = 'NO DATA', layout = 'plain'):
        self.msg.attach(MIMEText(data, layout, 'utf-8'))
        pass

    def addAttachment(self, filename):
        attachment = MIMEText(open(filename, 'rb').read(), 'base64', 'utf-8')
        attachment['Content-Type'] = 'application/octet-stream'
        attachment['Content-Disposition'] = 'attachment; filename="%s"' % (os.path.split(filename)[-1])
        self.msg.attach(attachment)
        pass

    def launch(self, server = 'smtp.exmail.qq.com', 
            user = 'monitor@mioji.com', 
            passwd = 'Mioji@2015Monitor'):
        try:
            client = smtplib.SMTP(server)
            client.login(user, passwd)
            client.sendmail(self.msg['From'], self.msg['To'].split(';'), self.msg.as_string())
            client.close()
            print 'success'
        except Exception, e:
            print str(e)


if __name__ == '__main__':  
    email = Email('test', ['huxuanzheng@mioji.com'])
    email.addContent('hello world')
    email.addAttachment('./file')
    email.launch()


