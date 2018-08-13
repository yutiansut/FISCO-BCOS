#/bin/bash
set -e
node_dir=
web3lib_dir=`readlink -f ../web3lib`
systemcontract_dir=`readlink -f ../systemcontract`
enable_guomi=0

this_script=$0

function LOG_ERROR()
{
    local content=${1}
    echo -e "\033[31m"${content}"\033[0m"
}

function LOG_INFO()
{
    local content=${1}
    echo -e "\033[32m"${content}"\033[0m"
}

function execute_cmd()
{
    local command="${1}"
    LOG_INFO "RUN: ${command}"
    eval ${command}
    local ret=$?
    if [ $ret -ne 0 ];then
        LOG_ERROR "FAILED execution of command: ${command}"
        exit 1
    else
        LOG_INFO "SUCCESS execution of command: ${command}"
    fi
}

yes_or_no()
{
    read -r -p "[Y/n]: " input
    case $input in
        [yY][eE][sS]|[yY])
            echo "Yes";;

        [nN][oO]|[nN])
            echo "No"
                ;;

        *)
        echo "No"
        ;;
    esac    
}

yes_go_other_exit()
{
    read -r -p "[Y/n]: " input
    case $input in
        [yY][eE][sS]|[yY])
            ;;

        [nN][oO]|[nN])
            exit 1
                ;;

        *)
        exit 1
        ;;
    esac    
}

help() {
    LOG_ERROR "${1}"
    LOG_INFO "Usage:"
    LOG_INFO "    -d  <node dir>            Node dir to be registered"
    LOG_INFO "Optional:"
    LOG_INFO "    -w  <web3lib dir>         Directory of web3lib"
    LOG_INFO "    -s  <systemcontract dir>  Directory of systemcontract"
    LOG_INFO "    -g                        Register guomi node"
    LOG_INFO "    -h                        This help"
    LOG_INFO "Example:"
    LOG_INFO "    bash $this_script -d /mydata/node0 "
    LOG_INFO "    bash $this_script -d /mydata/node0 -w ../web3lib/ -s ../systemcontract/ "
exit -1
}
while getopts "d:w:s:gh" option;do
	case $option in
    d) node_dir=$OPTARG;;
    w) web3lib_dir=$OPTARG;;
    s) systemcontract_dir=$OPTARG;;
    g) enable_guomi=1;; 
	h) help;;
	esac
done

[ -z $node_dir ] && help "Error: Please specify <node dir> using -d"
if [ ${enable_guomi} -eq 0 ];then
    [ ! -f "$node_dir/data/node.json" ] && help "Error: Missing $node_dir/data/node.json"
else
    [ ! -f "$node_dir/data/gmnode.json" ] && help "Error: Missing $node_dir/data/gmnode.json"
fi

check_node_running() {
    node_dir=$1
    pre_dir=`pwd`
    cd $node_dir

    #Is the node running?
    name=`pwd`/config.json
    agent_pid=`ps aux|grep "$name"|grep -v grep|awk '{print $2}'`
    if [ ! -n "$agent_pid" ]; then
        echo "Tips: Node is not running. Start now?"
        yes_go_other_exit

        echo "Starting node..."
        set +e && sh start.sh && set -e
        sleep 5
    fi
    cd $pre_dir
}

register_node_using_nodejs() {
    node_data_file=$1
    cd $systemcontract_dir
    execute_cmd "babel-node tool.js NodeAction register $node_data_file " 
    cd -
}

show_node() {
    cd ${systemcontract_dir}
    execute_cmd "babel-node tool.js NodeAction all"
}

check_node_running $node_dir
if [ ${enable_guomi} -eq 0 ];then
    register_node_using_nodejs  $node_dir/data/node.json
else
    register_node_using_nodejs  $node_dir/data/gmnode.json
fi
show_node
LOG_INFO "Register Node Success!"
