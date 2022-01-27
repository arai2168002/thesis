
<?php
  wp_enqueue_style( 'style_modal', get_stylesheet_directory_uri() . '/style_modal.css' );
?>

<div class="modal-window"><!-- Smodal -->
	<!--モーダル閉じる系-->
	<div class="modal-bg modal-close">
		<div class="modal-contents"></div>
	</div><!-- background -->

		<div class="modal-contents garbage-list">
			<!-- <div class="garbage-list"> -->
			<div class="garbage-division-area">
				<div class="garbage-group modal-button">
					<?php
						echo '<label for="' . gomi_category_checkbox_id('bed') . '">寝具類</label>' . "\n";
						echo '<label for="' . gomi_category_checkbox_id('bicycle') . '">自転車類</label>' . "\n";
						echo '<label for="' . gomi_category_checkbox_id('furniture') . '">家具類</label>' . "\n";
						echo '<label for="' . gomi_category_checkbox_id('appliance') . '">家電品</label>' . "\n";
						echo '<label for="' . gomi_category_checkbox_id('health') . '">健康器具</label>' . "\n";
						echo '<label for="' . gomi_category_checkbox_id('scrap') . '">廃材</label>' . "\n";
						echo '<label for="' . gomi_category_checkbox_id('other') . '">その他</label>' . "\n";
					?>
				</div>

				<div class="garbage-item" id="garbage-item">
					<!-- ここに選択された分類のごみが追加される -->
				</div>

				<!-- マウスオーバーしたときに変わる部分（テキスト） -->
				<div class="garbage-mouseover">
					<li id = "garbage-mouseover-name">名前：</li>
					<ul>
						<li id = "garbage-mouseover-category">カテゴリー：</li>
						<li id = "garbage-mouseover-condition">コンディション：</li>
					</ul>
				</div>
			</div>

			<div class="garbage-picked">
				<div class="list_name">選択リスト</div>
				<ul id="garbage-picked-list">
					<!-- ここに選択されたごみが追加される -->
				</ul>
			</div>
		<!--</div> -->
		</div>
			<input type="button" class="send-button modal-close" value="入力完了" >
</div>

<script language="javascript">

let jsonData = <?php  echo "'" . json_encode(get_gomi()) . "'"; ?>;
let url = <?php   echo "'" . get_gomi_pict_url(). "'";?>;
let jsonData_replace = jsonData.replace(/\n/g, '').replace(/<!-- wp:paragraph -->/g, '').replace(/<!-- \/wp:paragraph -->/g, '');
let jsonObject = JSON.parse(jsonData_replace);

//顧客データ （ゴミのid, 数） client_data = "[id1:num,id2:num‥]"の文字列
let client_data = [];

//let target_class = document.querySelector('[data-name = "trash_others"]'); //hidden切り替え 用の要素
let target_textarea = document.querySelector('[data-name = "trash_others"] textarea'); //uncategrized のtextarea
let target_visible = 	document.querySelector('[data-name = "trashes"] textarea'); // 見えるほうの textarea
let target_no_visible = document.querySelector('[data-name = "trash_data"] input'); // 見えないほうの input
let target_checkbox_un; //uncategrized 記憶用
//readonlyの属性を付与
target_visible.readOnly = true;

//trach_data を読んで，client_data に値を入れる
client_data_set();

//モーダルボタンを開けたり閉じたりする
jQuery(function($){

  //checkboxを押した時や値が変わった時のプロ
  $("input[type = checkbox]").each(function(){
    //checkboxの要素を取得・名前を変え
		let target_checkbox = document.getElementById($(this).attr("id"));

		//不明以外のcheckboxを押した時（モーダルの表示）
    if(target_checkbox.value != "uncategorized"){
      $(this).on("click", function(){
        list_switch(target_checkbox.value);//targetタグを初期表示
         $("body").css("overflow-y", "hidden");  // 本文の縦スクロールを無効
        $(".modal-window").fadeIn();
        return false;
      });
    }
		//不明のcheckbox
		else {
			target_checkbox_un = target_checkbox; //uncategorized の要素を記録
			//checkboxの値が変わった時の処理
			$(this).change(function(){
				//trueの場合99000を挿入し，falseの場合削除
				if($(this).prop("checked")) {
					pickClient("99000");
				}
				else client_data.splice(client_data.length - 1, 1);
			});
		}
  });

	//モーダルを閉じるプログラム
	$(".modal-close").on("click",function(){
		//textarea と checkbox の変更
		$("input[type = checkbox]").each(function(){
			let target_checkbox = document.getElementById($(this).attr("id"));
			if(target_checkbox.value != "uncategorized")
				Set_Textarea_Checkbox(target_visible, target_no_visible, target_checkbox);
		});
		//モーダルを閉じる
		$(".modal-window").fadeOut();
    $("body").css("overflow-y","auto");     // 本文の縦スクロールを有効
		return false;
	});

	//submit時に，"trash_others"に何も記入がなければ，チェックボックスの値を外す
	$("input[type = submit]").on("click", function(){
		//uncategrized textarea に何も書かれていなければ
		if(target_textarea.value == "") {
			//target_class.hidden = true;
			if(target_checkbox_un.checked){
				//target_checkbox_un.checked = false;
				client_data.splice(client_data.length - 1, 1);
			}
		}
		else {
			Set_Textarea_Checkbox(target_visible, target_no_visible, target_checkbox_un);
		}
	});
});

function client_data_set(){
  if (target_no_visible.value != ""){
    let str = target_no_visible.value.split(",");
    str.forEach((data, i) => {
      client_data.splice(i, 0, {id:data.split(":")[0], num:Number(data.split(":")[1])});
    });
  }
  else client_data = [];
  pick("99000");
}

//リスト内に表示するコンテンツを切り替える関数
function list_switch(category){
	//console.log(category);
	//親div
	let parent = document.getElementById("garbage-item");
	parent.innerHTML = ""; // 初期化
	//console.log(" category -> ", category); //デバッグ用

	Object.keys(jsonObject).forEach(key => {
		if(jsonObject[key].category == category){
			//console.log("モーダルオープン: target -> ", jsonObject[key]); //デバッグ用
			//テキスト
			let newElement = document.createElement("label"); //label要素作成
			newElement.setAttribute("for", jsonObject[key].id); //label要素にforを設定
			let newContent = document.createTextNode(jsonObject[key].name); //テキストノードを作成
			newElement.appendChild(newContent); //label要素にテキストノードを追加
			let brElement = document.createElement("br"); //br要素作成 (改行)
			newElement.appendChild(brElement); //label要素に改行を追加
			//newElement.setAttribute("id", jsonObject[key].id + "_label");

			//画像
			let img_element = document.createElement("img"); //img要素を作成
			img_element.src = url + "/" + jsonObject[key].picture; // 画像パス
			img_element.width  = 95;  // 横サイズ（px）
			img_element.height = 95; // 縦サイズ（px）
			newElement.appendChild(img_element); //label要素にimg要素を追加

			//マウスオーバー要素を作成
			newElement.addEventListener("mouseover", function() {
				change_text(key);
			}, false);

			parent.appendChild(newElement); //小要素を追加

			//　ボタンの要素を加える
			let input_element = document.createElement("input"); //input要素作成
			input_element.setAttribute("type", "button");
			input_element.setAttribute("id", jsonObject[key].id);
			input_element.setAttribute("onclick", "pick(this.id);");
			parent.appendChild(input_element);
		}
	});
}
	//ゴミを選択した時呼び出される関数
	function pick(key){
		pickClient(key); //顧客データの並び替え，個数

		let parent = document.getElementById("garbage-picked-list"); //親div
		parent.innerHTML =[];

    client_data.forEach((data, i) => {
      //追加する要素
      if(data.id != "99000"){
        let newElement_li = document.createElement("li"); //li要素作成
        let newElement_p = document.createElement("p"); //p要素作成
        let newElement_img = document.createElement("img"); //img要素作成
        let newContent = document.createTextNode(jsonObject[data.id].name + "\n"); //テキストノードを作成
        let num_p = document.createElement("p"); //p要素作成
        let plus_Button = document.createElement('button'); //選択リストに表示する要素数を増やすボタン
        let minus_Button = document.createElement('button'); //選択リストに表示する要素数を減らすボタン
        let delete_Button = document.createElement('button'); //選択リストに表示する要素数を消すボタン

        newElement_li.setAttribute("id", data.id + "_li"); //li要素にidを設定
        parent.appendChild(newElement_li); //小要素を追加
        newElement_li.style.display = "block";


        newElement_p.appendChild(newContent); //p要素にテキストノードを追加
        newElement_li.appendChild(newElement_p); //li要素にp要素を追加

        num_p.setAttribute("id", data.id + "_p"); //span要素にidを設定
        newElement_li.appendChild(num_p); //p要素にテキストノードを追加
        num_p.innerText = "個数：" + data.num;

        newElement_img.src = url + "/" + jsonObject[data.id].picture; // 画像パス
        newElement_li.appendChild(newElement_img); //label要素にimg要素を追加

        //num_span.setAttribute("id", data.id + "_span"); //span要素にidを設定
        //newElement_p.appendChild(num_span); //li要素にp要素を追加
        //num_span.innerText = data.num;

        plus_Button.value = data.id;
        plus_Button.innerHTML = "＋";
        newElement_li.appendChild(plus_Button); //li要素にボタン要素を追加
        //ボタンが押されたときの処理
        plus_Button.onclick = () => {
          let num_span = document.getElementById(data.id + "_p");
          index = client_data.findIndex(data => data.id === plus_Button.value);
          client_data[index].num += 1;
          num_span.innerText = "個数：" + client_data[index].num;
          //console.log(client_data); //デバッグ用
        }

        //-ボタン
        minus_Button.value = data.id;
        minus_Button.innerHTML = "ー";
        newElement_li.appendChild(minus_Button); //li要素にp要素を追加
        //ボタンが押されたときの処理
        minus_Button.onclick = () => {
          num_span = document.getElementById(data.id + "_p");
          index = client_data.findIndex(data => data.id === minus_Button.value);
          client_data[index].num -= 1;

          if(client_data[index].num > 0){
            num_span.innerText = "個数：" + client_data[index].num;
          }
          //要素が0になったときの処理
          else{
            client_data.splice(index, 1);
            newElement_li.style.display = "none";　//テキストとボタンを隠す
          }
          //console.log(client_data); //デバッグ用
        }

        //削除ボタン
        delete_Button.value = data.id;
        delete_Button.innerHTML = "リセット";
        newElement_li.appendChild(delete_Button); //li要素にp要素を追加
        //ボタンが押されたときの処理
        delete_Button.onclick = () => {
          num_span = document.getElementById(data.id + "_span");
          index = client_data.findIndex(data => data.id === delete_Button.value);
          client_data.splice(index, 1);
          newElement_li.style.display = "none";　//テキストとボタンを隠す
        }
      }
    });
	}

  //顧客データにあれば数を増やし，なければ顧客データにidと数追加する
  //数の小さい順に並ぶ
  function pickClient(key){
    let index = client_data.findIndex(data => data.id === key);
    if(index == -1){
      client_data.some(function(data, i) {
        //console.log("client_data.id", key, data.id);
        if(key < data.id){
          index = i;
          return true;
        }
        index = i + 1;
      });
      client_data.splice(index, 0, {id:key, num:1});
    }
    else {
			if(key != "99000") client_data[index].num++;
		}
  }

	//フーバーで要素見せる関数
	function change_text(key){
		//テキスト追加
		let change_p1 = document.getElementById("garbage-mouseover-name");
		let change_p2 = document.getElementById("garbage-mouseover-category");
		let change_p3 = document.getElementById("garbage-mouseover-condition");

		change_p1.innerHTML = "名前：" + jsonObject[key].name;
		change_p2.innerHTML = "カテゴリー：" + category_change(jsonObject[key].category);
		change_p3.innerHTML = "コンディション：" + jsonObject[key].condition;
	}

  function category_change(str){
    switch (str) {
      case "bed":
        return "寝具類";
      case "bicycle":
        return "自転車";
      case "furniture":
        return "家具類";
      case "appliance":
        return "家電品";
      case "health":
        return "健康器具";
      case "scrap":
        return "廃材";
      default:
        return "その他"
    }
  }

	//texarea（モーダル外）をセットし，ユーザが編集できなくする関数
	function Set_Textarea_Checkbox(target1, target2, target3) {
		console.log("前");
		console.log("target_textarea > ", target_checkbox_un.checked);
		console.log("target_textarea > ", target_textarea.value);
		console.log("target_visible > ", target_visible.value);
		console.log("target_no_visible > ", target_no_visible.value);

		let flag = false;

		//id取得・ターゲットの初期化
		target1.value = "";
		target2.value = "";

		client_data.forEach((item, i) => {
			//textarea に値を入れる
			if(item["id"] != "99000"){
				target1.value += jsonObject[item["id"]].name + " : " + item["num"];
				target2.value += item["id"] + ":" + item["num"];

				if(i < client_data.length - 1){
					if(client_data[i + 1].id != "99000") target1.value += "，";
					target2.value += ",";
				}
			}
			else {
				target2.value += item["id"] + ":" + item["num"];
			}

			//checkbox にチェックを入れる
			if(target3.value == jsonObject[item["id"]].category) flag = true;
		});
		if(flag == true) target3.checked = true;
		else 						 target3.checked = false;

		console.log("後");
		console.log("target_textarea > ", target_checkbox_un.checked);
		console.log("target_textarea > ", target_textarea.value);
		console.log("target_visible > ", target_visible.value);
		console.log("target_no_visible > ", target_no_visible.value);


	}

</script>
