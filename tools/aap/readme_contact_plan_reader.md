Hello there!
This document aims to provide information about the `aap_contact_plan_reader.py` Python tool that reads contact plans in ION format from a .txt file and configures uD3TN nodes. 

# Prerequisites

It is assumed that you have uD3TN installed.

# Contact plan ION format

Contact plan file contains information about which uD3TN nodes are reachable at what period of time.
The program only processes `a contact` commands. For documentation on the format, please refer to the manuals and examples provided with the [ION DTN protocol implementation](https://sourceforge.net/projects/ion-dtn/).

The **add contact** command schedules a period of data transmission from `sender_node` to `receiver_node` and has the following arguments:

`a contact <start_time> <end_time> <sender_node> <receiver_node> <transmit_rate>`

*Examples:*
- `a contact 2022/05/04-15:02:15 2022/05/04-16:00:15 1 2 10000` 
- `a contact +1 +3600 1 3 10000`

### **Remarks!**

Comments begin with `#` and are ignored.\
The nodes are represented as numbers in the `ipn` EID scheme form for ION.\
Time can be represented in two formats:
- Absolute time is written in form `yyyy/mm/dd−hh:mm:ss` 
- Relative time (a number of seconds following the current reference time) `+ss`

# Nodes configuration file

Nodes configuration file contains information about nodes that is needed in order to schedule the contacts.
Each line of the file has the following format:\
`<node name> <cla address> <type of aap port> <aap port address>`

- type of aap port may be `tcp` or `socket`
- aap port address in case of **tcp** is `ip` `port`
- aap port address in case of **socket** is `socket path`

*Examples :*
```
1 mtcp:localhost:4224 tcp localhost 4242
2 mtcp:localhost:4225 socket socketfile
```

# Starting a program

The paths to contact plan and nodes configuration files must be passed as arguments with `--plan` and `--nodes` \
Then the program can be started with command `python aap_contact_plan_reader.py --plan <path_to_contact_plan_file> --nodes <path_to_nodes_config_file>`

*Note!* [Python dependencies](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/posix_quick_start_guide.md#python-dependencies) should be available.

# Simple three-instance configuration

In this scenario we want to connect 3 uD3TN nodes with each other.

## Step 1 -Starting the uD3TN instances

Open a new shell in the uD3TN base directory and launch instance A as follows:\
`build/posix/ud3tn --eid ipn:1.0 --bp-version 7 --aap-port 4242 --cla "mtcp:*,4224"`

Open another shell and launch the second instance B as follows:\
`build/posix/ud3tn --eid ipn:2.0 --bp-version 7 --aap-port 4243 --cla "mtcp:*,4225"`

Now open yet another shell and launch the third instance C as follows:\
`build/posix/ud3tn --eid ipn:3.0 --bp-version 7 --aap-port 4244 --cla "mtcp:*,4226"` 

Now, all three µD3TN instances are up and running so we can continue with the next step!

## Step 2 -Creating nodes configuration file

Now we will create a `nodes_config.txt` file and copy the following code there:
```
1 mtcp:localhost:4224 tcp localhost 4242
2 mtcp:localhost:4225 tcp localhost 4243
3 mtcp:localhost:4226 tcp localhost 4244
```

From now on this file will contain the information about nodes needed for configuration.

## Step 3 -Creating contact plan file

Now we will create a `contact_plan.txt` file and copy the following text there:
```
a contact +1 +3600 1 2 100000
a contact +5 +3600 2 1 100000
a contact +10 +3600 1 3 10000
a contact +10 +3600 3 1 10000
a contact +15 +3600 3 2 10000
a contact +1 +3600 2 3 10000
```
Since we have two necessary files, we may proceed and put them in uD3TN base directory.

## Step 4 -Configure nodes

Now we are finally ready to configure the nodes!
Open another terminal in the uD3TN base directory and enter the following command:
```
python tools/aap/aap_contact_plan_reader.py --plan contact_plan.txt --nodes nodes_config.txt
```
Now you should see that some contacts were scheduled and some connections were accepted. 
