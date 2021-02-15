#!/bin/sh
## ==========================================================================
#   Licensed under BSD 2clause license See LICENSE file for more information
#   Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
## ==========================================================================

. ./mtest.sh

broker_name="/tpsmqd"
psmqs_name="/s"
psmqp_name="/p"

psmqd_pid=
psmqs_pid=

psmqs_stdout="tpsmqs.stdout"
psmqs_stderr="tpsmqs.stderr"
psmqp_stdout="tpsmqp.stdout"
psmqp_stderr="tpsmqp.stderr"
psmqd_stderr="tpsmqd.stderr"
psmqd_stdout="tpsmqd.stdout"
psmqd_log="tpsmqd.log"

psmqd_bin="../src/psmqd"
psmqs_bin="../src/psmq-sub"
psmqp_bin="../src/psmq-pub"




## ==========================================================================
#                  _                __           ____
#    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
#   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
#  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
# / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
#/_/
## ==========================================================================


## ==========================================================================
#   Generate random string.
#
#   $1 - number of characters to generate
## ==========================================================================


randstr()
{
    cat /dev/urandom | tr -dc 'a-zA-Z0-9' | tr -d '\0' | fold -w ${1} \
        | head -n 1 | tr -d '\n'
}


## ==========================================================================
## ==========================================================================


psmq_grep()
{
    string="${1}"
    file="${2}"
    i=0

    while true
    do
        if grep "${string}" "${file}" >/dev/null
        then
            # found
            return 0
        fi

        # not found yet
        sleep 0.1

        i=$((i + 1))
        if [ ${i} -eq 10 ]
        then
            # not found for specified ammount of time
            return 1
        fi
    done
}


## ==========================================================================
## ==========================================================================


start_psmqd()
{
    echo -n > ${psmqd_log}
    ${psmqd_bin} -l7 -p${psmqd_log} -r -b${broker_name} -m10 &
    psmqd_pid=${!}

    # wait for broker to startup

    while true
    do
        if grep "starting psmqd broker main loop" "${psmqd_log}" >/dev/null 2>&1
        then
            break
        fi
        sleep 0.1
    done

    # wait for last option to appear in the log file before
    # grepping for values

    psmq_grep "n/created" ${psmqd_log}

    psmq_msg_max=$(cat ${psmqd_log} | grep PSMQ_MSG_MAX \
        | cut -f3 -d] | cut -f3 -d\ )
}


## ==========================================================================
## ==========================================================================


start_psmqs()
{
    echo -n > "${psmqs_stdout}"
    ${psmqs_bin} -n${psmqs_name} -b${broker_name} -t/1 -t/2 -o${psmqs_stdout} \
        2> ${psmqs_stderr} &
    psmqs_pid=${!}

    # wait for psmq-sub to start

    while true
    do
        if grep "start receiving data" "${psmqs_stderr}" >/dev/null 2>&1
        then
            break
        fi
        sleep 0.1
    done
}


## ==========================================================================
## ==========================================================================


stop_psmqs()
{
    kill ${psmqs_pid}
    counter=0

    while true
    do
        if ! kill -s 0 ${psmqs_pid} 2>/dev/null
        then
            break
        fi

        sleep 0.1
        counter=$((counter + 1))

        if [ ${counter} -eq 10 ]
        then
            ###
            # program refuses to die, let's kill it again - order must be!
            #

            kill ${psmqs_pid}
            counter=0
        fi
    done
}


## ==========================================================================
## ==========================================================================


mt_prepare_test()
{

    return 0
}


## ==========================================================================
## ==========================================================================


mt_cleanup_test()
{
    echo -n > $psmqs_stdout
    echo -n > $psmqs_stderr
    echo -n > $psmqp_stdout
    echo -n > $psmqp_stderr
    echo -n > $psmqd_stderr
    echo -n > $psmqd_stdout
    echo -n > $psmqd_log
    return 0
}


## ==========================================================================
#                           __               __
#                          / /_ ___   _____ / /_ _____
#                         / __// _ \ / ___// __// ___/
#                        / /_ /  __/(__  )/ /_ (__  )
#                        \__/ \___//____/ \__//____/
#
## ==========================================================================


psmq_sub_print_help()
{
    ${psmqs_bin} -h > ${psmqs_stdout}
    mt_fail "psmq_grep \"listen to subscribed messages over psmq\" \
        \"${psmqs_stdout}\""
}
psmq_sub_print_version()
{
    ${psmqs_bin} -v > ${psmqs_stdout}
    mt_fail "psmq_grep \"by Michał Łyszczek <michal.lyszczek@bofc.pl>\" \
        \"${psmqs_stdout}\""
}
psmq_sub_broker_doesnt_exist()
{
    ${psmqs_bin} -n${psmqs_name} -b/surely-i-dont-exist 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/broker /surely-i-dont-exist doesn't exist\" \
        \"${psmqs_stderr}\""
}
psmq_sub_qname_too_long()
{
    psmqs_qname="/$(randstr ${psmq_msg_max})"
    ${psmqs_bin} -n${psmqs_qname} -b/surely-i-dont-exist 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/queue name is too long ($((psmq_msg_max + 1))), max is $((psmq_msg_max - 1))\" \
        \"${psmqs_stderr}\""
}
psmq_sub_topic_too_long()
{
    topic="/$(randstr ${psmq_msg_max})"
    ${psmqs_bin} -n${psmqs_name} -b${broker_name} -t${topic} 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"subscribe failed, topic ${topic} is too long\" \
        \"${psmqs_stderr}\""
}
psmq_sub_output_file_does_not_exit()
{
    ${psmqs_bin} -o/yea/sure/i/exist 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/failed to open file /yea/sure/i/exist for logging\" \
        \"${psmqs_stderr}\""
}
psmq_sub_topic_before_broker()
{
    ${psmqs_bin} -t/topic 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/subscribe failed, was -b set before -t option?\" \
        \"${psmqs_stderr}\""
}
psmq_sub_invalid_broker_name()
{
    ${psmqs_bin} -bno-wai 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/broker or queue name is invalid\" \
        \"${psmqs_stderr}\""
}
psmq_sub_invalid_queue_name()
{
    ${psmqs_bin} -nno-wai -b/valid 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/broker or queue name is invalid\" \
        \"${psmqs_stderr}\""
}
psmq_sub_invalid_topic()
{
    ${psmqs_bin} -n${psmqs_name} -b${broker_name} -t/ 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"subscribe failed, topic / is invalid\" \
        \"${psmqs_stderr}\""
}
psmq_sub_missing_argument()
{
    ${psmqs_bin} -b 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/option -b requires an argument\" \
        \"${psmqs_stderr}\""
}
psmq_sub_unknown_argument()
{
    ${psmqs_bin} -Q 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"f/unknown option -Q\" \
        \"${psmqs_stderr}\""
}
psmq_sub_no_arguments()
{
    ${psmqs_bin} 2> ${psmqs_stderr}
    mt_fail "psmq_grep \"missing -b option\" \"${psmqs_stderr}\""
}
psmq_sub_broker_go_down_before_client()
{
    # broker is started by now
    start_psmqs
    kill ${psmqd_pid}

    # psmq-sub should die with psmqd
    mt_fail "psmq_grep \"n/broker has closed the connection\" \
        \"${psmqs_stderr}\""

    # restart psmqd
    start_psmqd
}
psmq_pub_print_help()
{
    ${psmqp_bin} -h > ${psmqp_stdout}
    mt_fail "psmq_grep \"publish message over psmq\" \
        \"${psmqp_stdout}\""
}
psmq_pub_print_version()
{
    ${psmqp_bin} -v > ${psmqp_stdout}
    mt_fail "psmq_grep \"by Michał Łyszczek <michal.lyszczek@bofc.pl>\" \
        \"${psmqp_stdout}\""
}
psmq_pub_broker_doesnt_exist()
{
    ${psmqp_bin} -n${psmqp_name} -b/surely-i-dont-exist -t/t 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/broker /surely-i-dont-exist doesn't exist\" \
        \"${psmqp_stderr}\""
}
psmq_pub_qname_too_long()
{
    psmqp_qname="/$(randstr $((psmq_msg_max - 1)))"
    ${psmqp_bin} -n${psmqp_qname} -b${broker_name} -t/t 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/queue name is too long ($((psmq_msg_max))), max is $((psmq_msg_max - 1))\" \
        \"${psmqp_stderr}\""
}
psmq_pub_message_too_long()
{
    msg="$(randstr $((psmq_msg_max + 1)) )"
    ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/t -m${msg} 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/topic or message is too long\" \
        \"${psmqp_stderr}\""
}
psmq_pub_topic_too_long()
{
    topic="$(randstr $((psmq_msg_max + 1)) )"
    ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/${topic} -mmsg 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/topic or message is too long\" \
        \"${psmqp_stderr}\""
}
psmq_pub_invalid_broker_name()
{
    ${psmqp_bin} -bno-wai -t/t 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/broker or queue name is invalid\" \
        \"${psmqp_stderr}\""
}
psmq_pub_invalid_topic()
{
    ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t1 -mm 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/failed to publish to 1 invalid topic\" \
        \"${psmqp_stderr}\""
}
psmq_pub_invalid_queue_name()
{
    ${psmqp_bin} -nno-wai -b/valid -t/t 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/broker or queue name is invalid\" \
        \"${psmqp_stderr}\""
}
psmq_pub_missing_argument()
{
    ${psmqp_bin} -b 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/option -b requires an argument\" \
        \"${psmqp_stderr}\""
}
psmq_pub_unknown_argument()
{
    ${psmqp_bin} -Q 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/unknown option -Q\" \
        \"${psmqp_stderr}\""
}
psmq_pub_no_arguments()
{
    ${psmqp_bin} 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/missing broker name (-b) option\" \
        \"${psmqp_stderr}\""
}
psmq_pub_missing_b_argument()
{
    ${psmqp_bin} -n${psmqp_name} -t/t -mmsg 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/missing broker name (-b) option\" \
        \"${psmqp_stderr}\""
}
psmq_pub_missing_t_argument()
{
    ${psmqp_bin} -n${psmqp_name} -b${broker_name} -mmsg 2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/missing topic (-t) option\" \
        \"${psmqp_stderr}\""
}
psmq_pub_from_stdin()
{
    start_psmqs
    echo "test" | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1
    mt_fail "psmq_grep "test.." \"${psmqs_stdout}\""
    stop_psmqs
}
psmq_pub_from_stdin_max_line()
{
    start_psmqs
    # -2 since line consists of "\n\0"
    # -3 since topic is 3 bytes long "/1\0";
    msg="$(randstr $((psmq_msg_max - 2 - 3)) )"
    echo "${msg}" | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1
    mt_fail "psmq_grep $(echo ${msg} | cut -c-16) \"${psmqs_stdout}\""
    stop_psmqs
}
psmq_pub_from_stdin_too_long_line()
{
    # -2 since line consists of "\n\0"
    # -3 since topic is 3 bytes long "/1\0";
    # +1 because we want to exceed buffer to cause error
    msg="$(randstr $((psmq_msg_max - 2 - 3 + 1)) )"
    echo "${msg}" | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1 2> \
        ${psmqp_stderr}
    mt_fail "psmq_grep \"f/line is too long, max line is $((psmq_msg_max - 5))\" \
        \"${psmqp_stderr}\""
}
psmq_pub_from_stdin_too_long_topic()
{
    topic_len=${psmq_msg_max}
    topic_len=$((topic_len + 1))
    topic="/$(randstr ${topic_len} )"
    echo "msg" | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t${topic} 2> \
        ${psmqp_stderr}
    mt_fail "psmq_grep \"f/topic is too long, max is 254\" \
        \"${psmqp_stderr}\""
}
psmq_pub_from_stdin_invalid_topic()
{
    echo m | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t1 2> \
        ${psmqp_stderr}
    mt_fail "psmq_grep \"f/failed to publish to 1 invalid topic\" \
        \"${psmqp_stderr}\""
}
psmq_pub_from_stdin_multi_line()
{
    start_psmqs
    # -2 since line consists of "\n\0"
    # -3 since topic is 3 bytes long "/1\0";
    msg1="$(randstr $((psmq_msg_max - 3 - 2)) )"
    msg2="$(randstr $((psmq_msg_max - 3 - 2)) )"
    msg3="$(randstr 2)"
    msg4="$(randstr $((psmq_msg_max - 3 - 2)) )"
    msg5="$(randstr 2)"

    printf "%s\n%s\n%s\n%s\n%s\n" ${msg1} ${msg2} ${msg3} ${msg4} ${msg5} | \
        ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1 2> ${psmqp_stderr}
    mt_fail "psmq_grep $(echo ${msg1} | cut -c-16) \"${psmqs_stdout}\""
    mt_fail "psmq_grep $(echo ${msg2} | cut -c-16) \"${psmqs_stdout}\""
    mt_fail "psmq_grep $(echo ${msg3} | cut -c-16) \"${psmqs_stdout}\""
    mt_fail "psmq_grep $(echo ${msg4} | cut -c-16) \"${psmqs_stdout}\""
    mt_fail "psmq_grep $(echo ${msg5} | cut -c-16) \"${psmqs_stdout}\""
    stop_psmqs
}
psmq_pub_with_prio()
{
    start_psmqs
    msg="m"
    ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1 -m${msg} -p2
    mt_fail "psmq_grep \"topic: /1, priority: 2, paylen: 2, payload\" \
        \"${psmqs_stdout}\""
    mt_fail "psmq_grep \"${msg}.\" \"${psmqs_stdout}\""
    stop_psmqs
}
psmq_pub_with_invalid_prio()
{
    start_psmqs
    ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1 -mtest -p7812364 \
        2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/failed to publish, invalid prio 7812364\" \
        \"${psmqp_stderr}\""
    stop_psmqs
}
psmq_pub_from_stdin_with_prio()
{
    start_psmqs
    echo test | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1 -p2
    mt_fail "psmq_grep \"topic: /1, priority: 2, paylen: 6, payload\" \
        \"${psmqs_stdout}\""
    mt_fail "psmq_grep \"test.\" \"${psmqs_stdout}\""
    stop_psmqs
}
psmq_pub_from_stdin_with_invalid_prio()
{
    start_psmqs
    echo test | ${psmqp_bin} -n${psmqp_name} -b${broker_name} -t/1 -p7812364 \
        2> ${psmqp_stderr}
    mt_fail "psmq_grep \"f/failed to publish, invalid prio 7812364\" \
        \"${psmqp_stderr}\""
    stop_psmqs
}
psmqd_log_to_unavailable_file()
{
    ${psmqd_bin} -l7 -p/cant/log/here -b/mq/that/is/unavailable -m12345678 \
        2> ${psmqd_stderr}
    mt_fail "psmq_grep \"w/couldn't open program log file /cant/log/here\" \
        \"${psmqd_stderr}\""
    mt_fail "psmq_grep \"f/mq_open()\" \
        \"${psmqd_stderr}\""
}
psmqd_print_help()
{
    ${psmqd_bin} -h > ${psmqd_stdout}
    mt_fail "psmq_grep \"broker for publish subscribe over mqueue\" \
        \"${psmqd_stdout}\""
}
psmqd_print_version()
{
    ${psmqd_bin} -v > ${psmqd_stdout}
    mt_fail "psmq_grep \"by Michał Łyszczek <michal.lyszczek@bofc.pl>\" \
        \"${psmqd_stdout}\""
}
## ==========================================================================
## ==========================================================================


psmq_progs_simple_pub_sub()
{
    start_psmqs
    msg="$(randstr $((psmq_msg_max - 4)) )"
    ${psmqp_bin} -b${broker_name} -n${psmqp_name} -t/1 -m${msg}
    mt_fail "psmq_grep $(echo ${msg} | cut -c-16) \"${psmqs_stdout}\""
    stop_psmqs
}


## ==========================================================================
#                                             _
#                          ____ ___   ____ _ (_)____
#                         / __ `__ \ / __ `// // __ \
#                        / / / / / // /_/ // // / / /
#                       /_/ /_/ /_/ \__,_//_//_/ /_/
#
## ==========================================================================


start_psmqd

mt_run psmq_progs_simple_pub_sub
mt_run psmq_sub_print_help
mt_run psmq_sub_print_version
mt_run psmq_sub_broker_doesnt_exist
mt_run psmq_sub_qname_too_long
mt_run psmq_sub_output_file_does_not_exit
mt_run psmq_sub_topic_before_broker
mt_run psmq_sub_invalid_broker_name
mt_run psmq_sub_invalid_queue_name
mt_run psmq_sub_missing_argument
mt_run psmq_sub_unknown_argument
mt_run psmq_sub_no_arguments
mt_run psmq_sub_invalid_topic
mt_run psmq_sub_broker_go_down_before_client
mt_run psmq_pub_print_help
mt_run psmq_pub_print_version
mt_run psmq_pub_broker_doesnt_exist
mt_run psmq_pub_qname_too_long
mt_run psmq_pub_message_too_long
mt_run psmq_pub_invalid_broker_name
mt_run psmq_pub_invalid_topic
mt_run psmq_pub_invalid_queue_name
mt_run psmq_pub_missing_argument
mt_run psmq_pub_unknown_argument
mt_run psmq_pub_no_arguments
mt_run psmq_pub_missing_b_argument
mt_run psmq_pub_missing_t_argument
mt_run psmq_sub_topic_too_long

if [ ${psmq_msg_max} -gt 2 ]
then
    ###
    # since line is '\n' and then '\0' for sending a string,
    # it is not possible to send line when payload is 2
    #

    mt_run psmq_pub_from_stdin
    mt_run psmq_pub_from_stdin_max_line
    mt_run psmq_pub_from_stdin_too_long_line
    mt_run psmq_pub_from_stdin_multi_line
    mt_run psmq_pub_from_stdin_too_long_topic
    mt_run psmq_pub_from_stdin_invalid_topic
    mt_run psmq_pub_from_stdin_with_prio
    mt_run psmq_pub_from_stdin_with_invalid_prio
fi

if [ "$(uname)" != "QNX" ]
then
    ###
    # this test will not work on QNX, since it is impossible
    # there to pass such invalid parameters to crash the broker
    #

    mt_run psmqd_log_to_unavailable_file
fi

mt_run psmq_pub_with_invalid_prio
mt_run psmq_pub_with_prio
mt_run psmqd_print_help
mt_run psmqd_print_version

kill ${psmqd_pid}

mt_return
