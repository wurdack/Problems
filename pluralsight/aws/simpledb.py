__author__ = 'rwurdack'

import boto3
import pprint

def run():
    p = pprint.PrettyPrinter(indent=4)
    sdb = boto3.client('sdb')

    domain_name='Pluralsight_150216'

    # Create Domain
    #sdb.create_domain(DomainName=domain_name)

    # List available domains
    #domains = sdb.list_domains()
    #p.pprint(domains)

    # Put an item in the domain
    items = {'Fred'   : {"fat" : "True",  "lady" : "False", "redhead" : "False"},
             'Barney' : {"fat" : "False", "lady" : "False", "redhead" : "False"},
             'Wilma'  : {"fat" : "False", "lady" : "True" , "redhead" : "True"},
             'Betty'  : {"fat" : "False", "lady" : "True" , "redhead" : "False"}}

    # Trying to figure out how this api works
    #sdb.put_attributes(DomainName = domain_name,
    #                   ItemName='Flintstones',
    #                   Attributes=[{"Name": "Cartoon", "Value": "True"}])

    # Stuff attributes into db
    #for item in items:
    #    print "Adding {0}".format(item)
    #
    #    d = items[item]
    #    l = []
    #    for key in d:
    #        l.append({"Name" : key, "Value" : d[key]})
    #
    #    sdb.put_attributes(DomainName=domain_name, ItemName=item, Attributes=l)

    # Get them back out again
    result = sdb.select(SelectExpression="select * from {0} where fat='False'".format(domain_name))
    p.pprint(result)

    print "The following characters are not fat:"
    for item in result['Items']:
        print item['Name']

    # Put Fred on a diet
    #result = sdb.put_attributes(DomainName = domain_name,
    #                            ItemName = 'Fred',
    #                            Attributes = [{"Name": "fat", "Value": "False", "Replace": True}],
    #                            Expected = {"Name": "fat", "Value": "True"})



    p.pprint(result)

