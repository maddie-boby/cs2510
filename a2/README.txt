To run tests:
* Get, setup, and run zookeeper docker image
* CD into the test folder to run and build the image
  - $ docker build -t a2_leader_restart .
* run the image as follows:
  - $ docker run --name a2_leader_restart --link my-zk:zookeeper -d a2_leader_restart
* check the logs
  - $ docker logs <container-id>

It is important to link the containers so that the test container can see the zookeeper server.

Note: the leader kill and leader restart tests have 30 second pauses built in.
This is because zookeeper seems to take around 30 seconds to delete an ephemeral node and fire watches.