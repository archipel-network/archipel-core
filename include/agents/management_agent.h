#ifndef MANAGEMENT_AGENT_H_
#define MANAGEMENT_AGENT_H_

enum management_command {
	// Set the time of uPCN, argument: DTN time (64 bit)
	MGMT_CMD_SET_TIME,
};

int management_agent_setup(void);

#endif // MANAGEMENT_AGENT_H_
