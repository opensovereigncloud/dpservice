import pytest
import pytest, shlex, subprocess, time
from config import *


def pytest_addoption(parser):
	parser.addoption(
		"--build_path", action="store", default="../build", help="Provide absolute path of the build directory"
	)

@pytest.fixture(scope="package")
def build_path(request):
	return request.config.getoption("--build_path")

@pytest.fixture(scope="package")
def prepare_env(request, build_path):
	dp_service_cmd = build_path+"/src/dp_service -l 0,1 --vdev=net_tap0,iface="+pf0_tap+",mac=\""+pf0_mac+"\" "\
		"--vdev=net_tap1,iface="+pf1_tap+",mac=\""+pf1_mac+ "\" --vdev=net_tap2,"\
		"iface="+vf0_tap+",mac=\""+vf0_mac + "\" --vdev=net_tap3,iface="+vf1_tap+",mac=\""+vf1_mac+ "\" --vdev=net_tap4,iface="+vf2_tap+",mac=\""+vf2_mac + "\"  -- "\
		"--pf0="+pf0_tap+" --pf1="+pf1_tap+" --vf-pattern="+vf_patt+" --ipv6="+ul_ipv6+" --no-offload --no-stats"+" --op_env=scapytest"
	cmd = shlex.split(dp_service_cmd)
	process = subprocess.Popen(cmd, 
								stdout=subprocess.PIPE,
								universal_newlines=True)

	while True:
		output = process.stdout.readline()
		line = output.strip()
		
		if start_str in line:
			break
		return_code = process.poll()
		if return_code is not None:
			print('RETURN CODE', return_code)
			# Process has finished, read rest of the output 
			for output in process.stdout.readlines():
				print(output.strip())
			break
	def tear_down():
		process.terminate()
	request.addfinalizer(tear_down)
	return process

@pytest.fixture(autouse=True,scope="package")
def check_dpservice(prepare_env,build_path):
	return_code = prepare_env.poll()
	if return_code is not None:
		print("dp_service is not running")
		assert False
	
	return

@pytest.fixture(scope="package")
def add_machine(build_path):
	add_machine_cmd = build_path+"/test/dp_grpc_client --addmachine " + vm1_name + " --vni "+ vni + " --ipv4 " + vf0_ip + " --ipv6 " + vf0_ipv6
	add_machine_cmd2 = build_path+"/test/dp_grpc_client --addmachine " + vm2_name + " --vni "+ vni + " --ipv4 " + vf1_ip + " --ipv6 " + vf1_ipv6
	add_ipv4_route_cmd = build_path+"/test/dp_grpc_client --addroute --vni " + vni + " --ipv4 " + ov_target_pfx + " --length 24 --t_vni " + vni + " --t_ipv6 2a10:afc0:e01f:f408::1"
	add_ipv6_route_cmd = build_path+"/test/dp_grpc_client --addroute --vni " + vni + " --ipv6 2002::123 --length 128 --t_vni " + vni + " --t_ipv6 2a10:afc0:e01f:f408::1"
	subprocess.run(shlex.split("ip link set dev "+vf0_tap+" up"))
	subprocess.run(shlex.split("ip link set dev "+vf1_tap+" up"))
	subprocess.run(shlex.split("ip link set dev "+vf2_tap+" up"))
	subprocess.run(shlex.split("ip link set dev "+pf0_tap+" up"))
	subprocess.run(shlex.split(add_machine_cmd))
	subprocess.run(shlex.split(add_machine_cmd2))
	subprocess.run(shlex.split(add_ipv4_route_cmd))
	subprocess.run(shlex.split(add_ipv6_route_cmd))
	time.sleep(1)

