--TEST--
Hint test
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";

if (!method_exists('TokyoTyrantQuery', 'hint'))
	die("skip No hint available");
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrantTable(TT_TABLE_HOST, TT_TABLE_PORT);
$tt->vanish();

$tt->put('cherry',     array('color' => 'red'));
$tt->put('strawberry', array('color' => 'red'));
$tt->put('apple',      array('color' => 'green'));
$tt->put('lemon',      array('color' => 'yellow'));

$query = $tt->getQuery();
$query->addCond('color', TokyoTyrant::RDBQC_STREQ, 'red')->setOrder('color', TokyoTyrant::RDBQO_STRASC);

$query->search();
var_dump(strlen($query->hint()) > 10);

?>
--EXPECTF--
bool(true)