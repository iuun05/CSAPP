for i in $(seq -w 1 16); do
    echo "=== Testing trace$i.txt ==="
    ./sdriver.pl -t trace$i.txt -s ./tsh -a "-p" > /tmp/tsh.out
    ./sdriver.pl -t trace$i.txt -s ./tshref -a "-p" > /tmp/tshref.out
    diff -u <(sed -E 's/\([0-9]+\)/(PID)/g' /tmp/tsh.out) \
            <(sed -E 's/\([0-9]+\)/(PID)/g' /tmp/tshref.out)
    if [ $? -eq 0 ]; then
        echo "trace$i PASSED"
    fi
done