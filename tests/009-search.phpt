--TEST--
Table put variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
include 'config.inc.php';
skip_if_not_table();
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrantTable(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put(null, array('test' => 'abc123111'));
$tt->put(null, array('test' => 'cde456111'));

$query = $tt->getQuery();
$query->addCond('test', TokyoTyrant::RDBQ_CSTREQ, 'cde456111');
var_dump($query->search());

echo "------------------\n";

$query = $tt->getQuery();
$query->addCond('test', TokyoTyrant::RDBQ_CSTREQ, 'abc123111');
var_dump($query->search());

echo "------------------\n";

$query = $tt->getQuery();
$query->addCond('test', TokyoTyrant::RDBQ_CSTREW, '111');
var_dump($query->search());

echo "------------------\n";
	
?>
--EXPECTF--
array(1) {
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------
array(1) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
}
------------------
array(2) {
  [1]=>
  array(1) {
    ["test"]=>
    string(9) "abc123111"
  }
  [2]=>
  array(1) {
    ["test"]=>
    string(9) "cde456111"
  }
}
------------------