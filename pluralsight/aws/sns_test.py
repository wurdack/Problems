__author__ = 'rwurdack'

import boto3
import pprint
import random


###
### SNS stuff
###

TEST_TOPIC_NAME="com-angryrichard-sns-default"
TEST_TOPIC_ARN="arn:aws:sns:us-west-2:711559948283:com-angryrichard-sns-default"

def run():

    p = pprint.PrettyPrinter(indent=4)
    r = random.Random()

    p.pprint("SNS monkeying")
    p.pprint("")

    sns = boto3.client("sns")

    #resp = sns.set_topic_attributes(TopicArn=TEST_TOPIC_ARN, AttributeName="DisplayName", AttributeValue="The Core")
    #p.pprint(resp)
    #
    #resp = sns.list_topics()
    #p.pprint(resp)

    subject = "Hello from python"
    message = "Congratulations -- sent from python"
    resp = sns.publish(Message=message, TopicArn=TEST_TOPIC_ARN, Subject=subject)
    p.pprint(resp)


###
### SQS stuff
###

TEST_QUEUE_NAME="com-angryrichard-default2"

def run_sqs():
    p = pprint.PrettyPrinter(indent=4)
    r = random.Random()

    p.pprint("SNS/SQS monkeying")
    p.pprint("")

    sqs = boto3.client("sqs")

    #enqueue(sqs, TEST_QUEUE_NAME, "{0} hello from python".format(int(r.random()*1000)))
    #enqueue(sqs, TEST_QUEUE_NAME, "{0} another message!".format(int(r.random()*1000)))

    receive_msg(sqs, TEST_QUEUE_NAME)


def receive_msg(sqs, queue_name):
    resp = sqs.get_queue_url(QueueName=queue_name)
    url = resp['QueueUrl']

    resp = sqs.receive_message(QueueUrl=url, AttributeNames=['All'], WaitTimeSeconds=20)
    pprint.pprint(resp)

    message = resp['Messages'][0]
    print "Received: {0}".format(message['Body'])

    sqs.delete_message(QueueUrl=url, ReceiptHandle=message['ReceiptHandle'])


def enqueue(sqs, queue_name, msg):
    resp = sqs.get_queue_url(QueueName=queue_name)
    url = resp['QueueUrl']

    resp = sqs.send_message(QueueUrl=url, MessageBody=msg)
    return resp


#
# Dump out all the queues and their attributes
#
def list_queues(p, sqs):
    resp = sqs.list_queues()
    queues = resp['QueueUrls']

    # Pivot the attribute dictionary so that it is keyed off of queue name.
    queue_attrs = {}
    for q in queues:
        resp = sqs.get_queue_attributes(QueueUrl=q, AttributeNames=['All'])
        arn = resp['Attributes']['QueueArn']
        arn = arn[arn.rfind(':') + 1:]
        queue_attrs[arn] = resp['Attributes']
        queue_attrs[arn]['Url'] = q

    p.pprint(queue_attrs)

