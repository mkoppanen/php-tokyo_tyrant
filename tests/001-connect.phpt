--TEST--
Test connect variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT, array('persistent' => true));
$tt->connect(TT_HOST, TT_PORT, array('persistent' => false));
$tt->connect(TT_HOST, TT_PORT, array('persistent' => true));
$tt->connect(TT_HOST, TT_PORT, array('persistent' => true));
$tt->connect(TT_HOST, TT_PORT, array('persistent' => false));
$tt->stat();

echo "0 ok\n";

$tt1 = new TokyoTyrant(TT_HOST, TT_PORT, array('persistent' => true));
$tt1->stat();

echo "1 ok\n";

$tt2 = new TokyoTyrant(TT_HOST, TT_PORT, array('persistent' => false, 'timeout' => 2.0, 'reconnect' => false));
$tt2->stat();

echo "2 ok\n";

$tt3 = new TokyoTyrant(TT_HOST, TT_PORT, array('persistent' => true, 'timeout' => 42.0));
$tt3->stat();

echo "3 ok\n";

$tt4 = new TokyoTyrant();
$uri = sprintf("tcp://%s:%d?persistent=0&timeout=1.0&reconnect=1", TT_HOST, TT_PORT);

$tt4->connectUri($uri);
$tt4->stat();

echo "4 ok\n";

$tt5 = new TokyoTyrant();
unset($tt5);

echo "5 ok\n";

$tt6 = new TokyoTyrant();
$uri = sprintf("tcp://%s:%d", TT_HOST, TT_PORT);

$tt6->connectUri($uri);
$tt6->stat();

echo "6 ok\n";

?>
--EXPECT--
0 ok
1 ok
2 ok
3 ok
4 ok
5 ok
6 ok