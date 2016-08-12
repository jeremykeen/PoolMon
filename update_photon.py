from sseclient import SSEClient 
import requests, re, json

access_token = "<token_here>"
publish_prefix_head = "poolMon" # for subscribing to incoming messages, e.g. myFarm
publish_prefix = "poolMon/pHSensor" # e.g. myFarm/waterSystem
messages = SSEClient('https://api.spark.io/v1/events/' + publish_prefix_head + '?access_token=' + access_token)
r = requests.post('https://api.particle.io/v1/devices/events', data = {"name":publish_prefix + "/update", "data":"true", "private":"false", "ttl":"60", "access_token":access_token})
if r.json()['ok']==True:
    print 'successfully sent update request'


with open('recorded messages.txt', 'w') as record:
    for msg in messages:
        event = str(msg.event).encode('utf-8')
        data = str(msg.data).encode('utf-8')
        if re.search('poolMon', event):
            dataJson = json.loads(data)
            if event == publish_prefix + '/online' and dataJson['data'] == "true":
                r = requests.post('https://api.particle.io/v1/devices/events', data = {"name":publish_prefix + "/update", "data":"true", "private":"false", "ttl":"60", "access_token":access_token})
                if r.json()['ok']==True:
                    print 'successfully sent update request'
            if event == publish_prefix + '/updateConfirm':
                if dataJson['data'] == 'waiting for update':
                    print 'device waiting for update...'
                if dataJson['data'] == 'not waiting for update':
                    print 'device no longer waiting for update.'
            if event == publish_prefix + '/sleep':
            	print 'device sleeping'
