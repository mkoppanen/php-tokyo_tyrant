Running tt for the session handler:

ttserver -port 2000 -ext /path/to/expire.lua -extpc expire 30.0 '/tmp/test0.tct#idx=ts:dec'


Configuring session.save_handler and session.save_path:

session.save_handler=tokyo_tyrant
session.save_path="tcp://hostname1:2000,tcp://hostname2:2000"


Putting it all together:

tokyo_tyrant.session_salt="4aDFSrtwdgSDRwerdfSDt3AGsh"
session.save_handler=tokyo_tyrant
session.save_path="tcp://hostname1:2000,tcp://hostname2:2000"

Why is the salt needed?

The session handler creates the following style session id:

8b0e27a823fa4a6cf7246945b82c1d51-a5eadbbed1f2075952900628456bfd6830541629-0-5460

Parts: 
  1. Random session id
  2. Checksum 
  3. Node id 
  4. Primary key

The checksum contains SHA1 sum of the node id, primary key, session id and the salt which is known only on the server side. This allows quick mapping of session id to node and primary key since there is no need to do a search. SHA1 should be 'safe enough'.

During session id regeneration only the parts 1 and 2 change but the mapping to the node and pk stays.