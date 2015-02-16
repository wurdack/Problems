__author__ = 'rwurdack'

import boto3
import pprint

printer = pprint.PrettyPrinter(indent=4)

s3 = boto3.client('s3')

print "Query buckets"
resp = s3.list_buckets()
if resp["ResponseMetadata"]["HTTPStatusCode"] != 200:
    print "Error in response"
    printer.pprint(resp)

print "Upload image to bucket"
with open("/Users/rwurdack/Pictures/seahawks.jpg", "rb") as ifp:
    resp = s3.put_object(Bucket="walrus150207", Key="seahawks.jpg", ContentType="image/jpeg", Body=ifp)

printer.pprint(resp)