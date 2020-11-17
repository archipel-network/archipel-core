-- Wireshark dissector for uD3TN AAP
-- This script may be used to view the on-wire AAP communication in Wireshark.
-- Use as follows: wireshark -Xlua_script:/path/to/tools/aap.lua

ud3tn_aap = Proto("ud3tn_aap", "uD3TN Application Agent Protocol")

local f_version = ProtoField.uint8(
	"ud3tn_aap.version", "Version", base.HEX,
	nil,
	0xf0
)

local f_type_names = {
	[0x0] = "ACK",
	[0x1] = "NACK",
	[0x2] = "REGISTER",
	[0x3] = "SENDBUNDLE",
	[0x4] = "RECVBUNDLE",
	[0x5] = "SENDCONFIRM",
	[0x6] = "CANCELBUNDLE",
	[0x7] = "WELCOME",
	[0x8] = "PING",
}

local f_type = ProtoField.uint8(
	"ud3tn_aap.type", "Type", base.HEX,
	f_type_names,
	0x0f
)

local f_eid_length = ProtoField.uint16(
	"ud3tn_aap.eid_length",
	"EID Length",
	base.DEC)

local f_eid = ProtoField.string("ud3tn_aap.eid", "EID", base.ASCII)


local f_bundle_length = ProtoField.uint64(
	"ud3tn_aap.bundle_length",
	"Bundle Payload Length",
	base.DEC)

local f_bundle_data = ProtoField.bytes("ud3tn_aap.bundle", "Bundle Payload")

local f_bundle_id_sent = ProtoField.uint64(
	"ud3tn_aap.bundle_id_sent",
	"Sent Bundle ID",
	base.HEX
	-- base.NONE,
	-- frametype.REQUEST
)

local f_bundle_id_cancel = ProtoField.uint64(
	"ud3tn_aap.bundle_id_cancel",
	"Cancel Bundle ID",
	base.HEX
	-- base.NONE,
	-- frametype.RESPONSE
)

ud3tn_aap.fields = {
	f_version,
	f_type,
	f_eid_length,
	f_eid,
	f_bundle_length,
	f_bundle_data,
	f_bundle_id_sent,
	f_bundle_id_cancel,
}

local function get_aap_eid_msg_length(tvb, pinfo, offset)
	if tvb:reported_len() < offset + 1 + 2 then
		return 0
	end

	local eid_length = tvb(offset + 1, 2):uint();
	return 1 + 2 + eid_length;
end

local function get_aap_bundle_msg_length(tvb, pinfo, offset)
	local len = get_aap_eid_msg_length(tvb, pinfo, offset)
	if len == 0 then
		return 0
	end

	if tvb:reported_len() < offset + len + 8 then
		return 0;
	end

	local bundle_length = tvb(offset + len , 8):uint64();

	-- this is fugly
	return (len + 8 + bundle_length):tonumber();
end

local function get_aap_bundle_id_msg_length(tvb, pinfo, offset)
	return 1 + 8
end

local function get_aap_length(tvb, pinfo, offset)
	local type_ = tvb(offset, 1):bitfield(4, 4);
	-- ACK, NACK and PING are header-only
	if (type_ == 0x0 or type_ == 0x1 or type_ == 0x8) then
		return 1
	elseif (type_ == 0x2 or type_ == 0x7) then
		return get_aap_eid_msg_length(tvb, pinfo, offset)
	elseif (type_ == 0x3 or type_ == 0x4) then
		-- message with EID + message payload
		return get_aap_bundle_msg_length(tvb, pinfo, offset)
	elseif (type_ == 0x5 or type_ == 0x6) then
		-- message with bundle ID payload
		return get_aap_bundle_id_msg_length(tvb, pinfo, offset)
	end
	return 1
end


local function dissect_aap_eid(tvb, pinfo, tree, offset)
	local eid_length = tvb(offset, 2):uint()
	tree:add(f_eid_length, tvb(offset, 2))
	tree:add(f_eid, tvb(offset + 2, eid_length))
	return 2 + eid_length
end


local function dissect_aap_bundle(tvb, pinfo, tree, offset)
	local bundle_length = tvb(offset, 8):uint64();
	tree:add(f_bundle_length, tvb(offset, 8))
	tree:add(f_bundle_data, tvb(offset + 8, bundle_length:tonumber()))
	return 8 + bundle_length
end


local function dissect_aap(tvb, pinfo, tree)
	pinfo.cols.protocol = "UD3TN AAP";

	local subtree = tree:add(ud3tn_aap, tvb(0))
	subtree:add(f_version, tvb(0, 1))
	subtree:add(f_type, tvb(0, 1))

	local type_ = tvb(0, 1):bitfield(4, 4);

	subtree:append_text(": " .. f_type_names[type_])

	if (type_ == 0x0 or type_ == 0x1 or type_ == 0x8) then
		-- ACK/NACK/PING, no payload
		return
	elseif (type_ == 0x2 or type_ == 0x7) then
		-- REGISTER/WELCOME
		dissect_aap_eid(tvb, pinfo, tree, 1);
	elseif (type_ == 0x3 or type_ == 0x4) then
		-- SEND/RECVBUNDLE
		local offset = 1;
		offset = offset + dissect_aap_eid(tvb, pinfo, subtree, offset);
		dissect_aap_bundle(tvb, pinfo, subtree, offset);
	elseif (type_ == 0x5) then
		-- SENDCONFIRM
		subtree:add(f_bundle_id_sent, tvb(1, 8))
	elseif (type_ == 0x7) then
		-- CANCELBUNDLE
		subtree:add(f_bundle_id_cancel, tvb(1, 8))
	end
end


function ud3tn_aap.dissector(tvb, pkt, root)
	dissect_tcp_pdus(
		tvb, root, 1, get_aap_length, dissect_aap
	)
end

local dissector_table = DissectorTable.get("tcp.port")
dissector_table:add(4242, ud3tn_aap)
