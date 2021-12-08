import sshtunnel, mysql.connector, json, os, sys

# Parse args
if len (sys.argv) < 2:
  print ("ERROR: Not enough arguments. Query file path must be supplied")
  sys.exit (1)

FILE_NAME = sys.argv[1]
print ("FILE_NAME: ", FILE_NAME)

HOST = os.environ.get ('DATABASE_HOST')
SSH_USER = os.environ.get ('DATABASE_SSH_USER')
SSH_PASS = os.environ.get ('DATABASE_SSH_PASS')
SQL_USER = os.environ.get ('DATABASE_SQL_USER')
SQL_PASS = os.environ.get ('DATABASE_SQL_PASS')
SQL_BASE = os.environ.get ('DATABASE_SQL_DATABASE')

for var in [HOST, SSH_USER, SSH_PASS, SQL_USER, SQL_PASS, SQL_BASE]:
  if not var:
    print ("ERROR: Incorrect env vars set for usage")
    sys.exit (1)

# Open SSH tunnel and forward port
sshtunnel.SSH_TIMEOUT = 5.0
sshtunnel.TUNNEL_TIMEOUT = 5.0

server = sshtunnel.SSHTunnelForwarder(
    HOST,
    ssh_username=SSH_USER,
    ssh_password=SSH_PASS,
    remote_bind_address=('127.0.0.1', 3306)
)

server.start()
print("Server opened forwarding port {}".format (server.local_bind_port))

# Open the database connection
mydb = mysql.connector.connect(
  host="127.0.0.1",
  port=server.local_bind_port,
  user=SQL_USER,
  password=SQL_PASS,
  database=SQL_BASE
)

# Perform the query returning results as a dict
f = open (FILE_NAME, "r")
queryContents = f.read()

cursor = mydb.cursor (dictionary=True)
cursor.execute (queryContents)

# Fetch all results
results = cursor.fetchall()
print (results)

# Close the server
server.stop()