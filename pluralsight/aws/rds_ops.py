__author__ = 'rwurdack'

db_instance='pluralsight150214-2'

db_host='pluralsight150214-2.cdxlrpre3suq.us-west-2.rds.amazonaws.com'
db_name='PluralsightDemoDB'
db_user='wurdack'
db_pwd='Ba$ketb011'


import pprint
p = pprint.PrettyPrinter(indent=4)


if False:
    import boto3
    rds = boto3.client('rds')

    print "original"
    resp = rds.describe_db_instances(DBInstanceIdentifier=db_instance)
    p.pprint(resp)

    print "modify db instance"
    resp = rds.modify_db_instance(DBInstanceIdentifier=db_instance, PreferredMaintenanceWindow='mon:14:14-mon:15:15')
    p.pprint(resp)

    print "new"
    resp = rds.describe_db_instances(DBInstanceIdentifier=db_instance)
    p.pprint(resp)

### Database stuff
import pg8000

print "connect to {0} on {1}".format(db_name, db_host)
cn = pg8000.connect(user=db_user, host=db_host, database=db_name, password=db_pwd)
p.pprint(cn)
cursor = cn.cursor()

if True:

    cursor.execute("select * from information_schema.tables")
    res = cursor.fetchall()
    for row in res:
        print row[2]

