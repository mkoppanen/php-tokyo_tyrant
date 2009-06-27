--TEST--
Session handler
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

ini_set("tokyo_tyrant.session_salt", sha1(mt_rand(1, 100)));
ini_set("session.save_path", sprintf("tcp://%s:%d", TT_TABLE_HOST, TT_TABLE_PORT));
ini_set("session.save_handler", "tokyo_tyrant");

session_start();
$_SESSION['test'] = 1;


echo session_id() . "\n";
?>
--EXPECTF--
%s-%s-%d-%d