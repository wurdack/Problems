__author__ = 'rwurdack'

import boto3
import pprint

TABLE_NAME='PlurasightDynamoRange'

def run():
    p = pprint.PrettyPrinter(indent=4)
    ddb = boto3.client('dynamodb')

    #queries

    key_conditions = { "Id":            {"ComparisonOperator":"EQ", "AttributeValueList":[{"S":"100"   }]},
                       "ActivityStamp": {"ComparisonOperator":"GT", "AttributeValueList":[{"N":"200101"}]}}

    #resp = ddb.query(TableName=TABLE_NAME,
    #                 KeyConditions=key_conditions)
    #
    #p.pprint(resp)

    if False:
        scan_filter = { "Activity": {"ComparisonOperator":"CONTAINS", "AttributeValueList":[{"S":"Search"}]}}
        resp = ddb.scan(TableName=TABLE_NAME, ScanFilter = scan_filter)

    else:
        exp_attr_vals = {":s":{"S":"Search"}}
        filter_expression = "contains(Activity, :s)"
        resp = ddb.scan(TableName=TABLE_NAME, FilterExpression=filter_expression, ExpressionAttributeValues=exp_attr_vals)

    p.pprint(resp)

def part_c():
    item = {
        "Id": {"S":"1003"},
        "Course": {"S":"Biztalk-o-rama"}
    }

    resp = ddb.put_item(TableName = TABLE_NAME,
                        Item = item)

    p.pprint(resp)


def part_b():
    item = {
        "Id": {"S":"1219"}
    }

    resp = ddb.put_item(TableName = TABLE_NAME,
                        Item = item)

    p.pprint(resp)

def part_a(ddb, p):
    resp = ddb.list_tables()
    #p.pprint(resp)

    table_names=resp['TableNames']
    for table in table_names:
        print "getting description of {0}".format(table)
        resp = ddb.describe_table(TableName=table)
        p.pprint(resp)
