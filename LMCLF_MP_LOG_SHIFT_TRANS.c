#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#define P 4 
//#define P 2  				//プロセッサ数
//#define P 1  				//プロセッサ数
//#define MAX 1000000000 //起動していない時
//#define alpha 1000.0		//換算レート
#define NIL ((List)0)
#define S 1024
#define FIXEDPOINT 2	//固定小数に変換するために2進数においてシフトする回数

typedef long long FIXPOINTDECIMAL;		//long long型をまとめて宣言
#define ITFD(x) ((FIXPOINTDECIMAL)(x << FIXEDPOINT))	//int型の数値を固定小数に変換



FIXPOINTDECIMAL MAX=2147483647; //起動していない時
int TN = 0;					//タスク数
int valTN=P*2;				//評価値により厳選されるタスク数
double dead_max = 0;		//相対デッドラインの最大値
FIXPOINTDECIMAL rand_memory[S][S];		//消費メモリ増分
double ET[S];				//Taskの1ステップの実行時間の平均（本実験では1ステップ1単位時間で実行）
int schedule[S];			//Taskがスケジュールされるとき1，そうでないとき0

int state[S];				//Taskが起動している場合は値を1,起動していない場合は0
int finish[S];				//タスクの処理がすべて終了していれば1，そうでなければ0

int step[S];				//現在のステップ

double save_laxity[S];		//Taskが終了した時のLaxity Time

int Worst_Memory = 0;		//最悪メモリ消費量
int Current_Memory = 0;		//現在のメモリ消費量

int deadline_miss_task[S];	//デッドラインミスの数
int Deadline_Miss = 0;		//デッドラインミスの数の総和

FIXPOINTDECIMAL alpha=ITFD(1),alphadiff=0; //パラメータα(初期値)0、αの修正差分

/*sort用変数*/
FIXPOINTDECIMAL sort_priority[S];
int sort_num_LMCLF[S];

int hook[S];				//一時停止している（フックがかかっている）場合1，そうでなければ0


/*構造体の要素*/
/*Task番号(Task1は0),相対デッドライン,最悪実行時間,起動時刻,実行済みの時間,余裕時間,選ばれたタスク,選ばれなかったタスク*/
typedef struct{
	int Number;
	double Relative_Deadline;
	double WCET;
	double Release_Time;
	double Run_Time;
	double Laxity_Time;
}data;



typedef struct cell {
  FIXPOINTDECIMAL element;
  struct cell *next;
} Cell,*List;



data task_data[S];

void *thread_Tasks(void *num);			//タスク
void *thread_LMCLF();					//LMCLF Schedular

void LMCLF();  							//優先度関数値をソートする関数
void memory_recode(int Memory);			//最悪メモリ消費量を記憶する関数

void calc();							//Taskの処理
void load();							//計算負荷

int pthread_yield(void);				//コンパイラに警告を出させないためのプロトタイプ宣言である.なお,この関数はPOSIXでは非標準であり,sched_yield()を使うのが正しいとある.

int get_digit(int n);                //桁数を知る

List createList(void);				//空のリストを作成
List insertList(FIXPOINTDECIMAL element,List l);	//リストの先頭に要素を追加する
int nullpList(List l);				//リストが空かどうかを返す
int headList(List l);				//リストの先頭要素を返す
List tailList(List l);				//リストの先頭要素を削除
int tailarray(List l,int i);		//
List appendList(List l1,List l2);	//リストl1の末尾にlリストl2を連結
int iList(List l1,int i);			//リストのi番目の要素を返す
void printList(List l);				//リストの要素を表示
void fprintList(List l);            //リストの全要素をファイルに出力
void fprintFPList(List l);			//リストの全要素文字列をファイルに出力
int lengthList(List l);				//リストの長さを取得
List firstnList(List l,unsigned int n);		//リストの先頭からn番目までの要素を削除
List restnList(List l,unsigned int n);		//リストの末尾からn番目までの要素を削除
int memberList(int element,List l);     //リストにその要素が含まれているかどうかを調べる関数
void freeList(List l);		//リストのメモリ解放
List copyList(List l);		//リストの複製を生成
int minList(List l,int tasknum1,int tasknum2,FIXPOINTDECIMAL min);	//リストの評価値の最小値のタスク番号を返す
List ideleatList(List l1,List l2,int i);		//先頭からi番目の要素を書き換える
List setList(List sourceList,List subsetList,int begin,int end);		//組み合わせの全パターンを格納したリストを返す
int calcNumOfCombination(int n, int r);		//組み合わせの総数を返す
int shiftoperationtimes(FIXPOINTDECIMAL n);		//シフト演算により桁数を取得
FIXPOINTDECIMAL FTFD(double x);				//double型の数値を固定小数に変換
char *FixPointDecimalToString(FIXPOINTDECIMAL x);		//固定小数の値を整数部分と小数部分とで文字列に変換



/*スレッド間で変数を保護するmutex*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/*スレッド間で状態を判断するcond*/
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


int main(void) {
	clock_t start_clock, end_clock;

	fprintf(stderr, "\n-------------LMCLF Scheduling in %d-Processor Environment-------------\n", P);
	start_clock = clock();

	int i = 0, j = 0, k1 = 0, k2 = 0;
	
	pthread_t Task[S];
	pthread_t LMCLF_scheduler;
	
	/*ファイル読み込み用変数*/
	char file_mem[256];								//消費メモリ増分のファイル
	char *file_dl = "rand_period_tasks.txt";		//周期のファイル（本実験では周期=相対デッドライン）
	FILE *fMemory;
	FILE *fdl;
	int ret, x;
	
	int ptask;										//仮のタスク数
	int pdeadline[S];								//仮のデッドライン
	int period_count = 0;							//最小公倍数までのステップで何度リリースされるか
	
	
	pthread_mutex_lock(&mutex);
	/*ファイル読み込み*/
	fdl = fopen(file_dl,"r");
	ptask = 0;
	while(ret = fscanf(fdl,"%d",&x) != EOF){
		pdeadline[ptask] = x;
		//fprintf(stderr, "Relative Deadline [Task%d] = %5.0lf\n", ptask+1, task_data[ptask].Relative_Deadline);
		ptask++;
	}
	
	/*相対デッドラインの最大値*/
	for(i=0;i<ptask;i++){
		if(dead_max < pdeadline[i])
			dead_max = pdeadline[i];
	}
	
	/*タスク数の決定*/
	for(j=0;j<ptask;j++){
		TN = TN + ceil(dead_max / pdeadline[j]);
		//fprintf(stderr,"%lf\n", ceil(dead_max / pdeadline[j]));
	}
	
	//fprintf(stderr,"%d\n", TN);
	
	/*Task_Dataの初期化*/
	for(i=0;i<TN;i++){
		task_data[i].Number = i;
		task_data[i].WCET = 0;
		task_data[i].Release_Time = 0;
		task_data[i].Run_Time = 0;
		task_data[i].Laxity_Time = MAX;
		
		state[i] = 0;
		deadline_miss_task[i] = 0;
		schedule[i] = 0;
		step[i] = 0;
		ET[i] = 1;
		save_laxity[i] = 0;
		hook[i] = 0;
		finish[i] = 0;
	}
	
	/*ファイル読み込み*/
	k2 = 0;
	for(i=0;i<ptask;i++){
		
		period_count = ceil(dead_max / pdeadline[i]);
		
		for(k1=0;k1<(int) period_count;k1++){
			
			/*消費メモリ増分*/
			sprintf(file_mem,"rand_memory_task%d.txt", i+1);
			fMemory = fopen(file_mem,"r");
			j = 0;
			while(ret = fscanf(fMemory,"%d",&x) != EOF){
				rand_memory[k2][j] = x;
				j++;
			}
			
			/*周期*/
			task_data[k2].Relative_Deadline = pdeadline[i];
			
			/*リリース*/
			task_data[k2].Release_Time = k1 * pdeadline[i];
			
			/*WCET*/
			task_data[k2].WCET = j;
			
			k2++;
		}
	}
	
	/*出力*/
	for(i=0;i<TN;i++)
		fprintf(stderr, "Relative Deadline [Task%d] = %5.0lf\n", i+1, task_data[i].Relative_Deadline);
	for(i=0;i<TN;i++)
		fprintf(stderr, "Release Time [Task%d] = %5.0lf\n", i+1, task_data[i].Release_Time);
	for(i=0;i<TN;i++)
		fprintf(stderr, "WCET [Task%d] = %5.0lf\n", i+1, task_data[i].WCET);
	pthread_mutex_unlock(&mutex);

	/*スレッドの生成*/
	pthread_create(&LMCLF_scheduler, NULL, thread_LMCLF, NULL);

	/*スレッドの停止*/
	pthread_join(LMCLF_scheduler,NULL);
	
	/*デッドラインミスしたTaskをカウント*/
	for(i=0;i<TN;i++){
		if(deadline_miss_task[i] > 0)
			Deadline_Miss++;
	}

	fprintf(stderr, "\n");
	/*終了時の余裕時間*/
	for(i=0;i<TN;i++){
		save_laxity[i] += 0;
		fprintf(stderr, "When Finished Laxity Time of Task%d = %5.3lf\n", task_data[i].Number+1, save_laxity[i]);
	}
	
	fprintf(stderr, "The Number of Deadline Miss = %d\n", Deadline_Miss);
	fprintf(stderr, "Worst Case Memory Consumption (LMCLF) = %d\n", Worst_Memory);
	
	/*ログの出力*/
	char log[256];
	FILE *fpout_LMCLF;
	sprintf(log, "datafile_LMCLF_%d.txt", alpha);
	fpout_LMCLF = fopen(log, "a");
	fprintf(fpout_LMCLF, "%d\t", Worst_Memory);		
	fprintf(fpout_LMCLF, "%d\r\n", Deadline_Miss);
	fclose(fpout_LMCLF);

	end_clock = clock();
	fprintf(stderr, "\n-------------LMCLF Scheduling in %d-Processor Environment-------------\n\n", P);

	fprintf(stderr, "\n clock：%f \n", (double)(end_clock - start_clock)/CLOCKS_PER_SEC);

	return 0;
}


/*タスク*/
void *thread_Tasks(void *num){

	int *task_number = (int *)num;	//タスクの識別子
	
	int i;							//カウント用変数
	
	fprintf(stderr, "Task%d next starts\n", *task_number+1);
	pthread_mutex_lock(&mutex);
	state[*task_number] = 1;
	hook[*task_number] = 1;			//タスクにフックをかける
	pthread_mutex_unlock(&mutex);
	
	/*Taskの余裕時間を計算*/
	task_data[*task_number].Laxity_Time = task_data[*task_number].Relative_Deadline - task_data[*task_number].WCET + task_data[*task_number].Run_Time;
	task_data[*task_number].Laxity_Time += 0;
	fprintf(stderr, "Laxity Time [Task%d] = %5.3lf\n", *task_number+1, task_data[*task_number].Laxity_Time);
	
	/*既にデッドラインミスしている可能性があるため*/
	if(task_data[*task_number].Laxity_Time < -0.0)
		deadline_miss_task[*task_number]++;
	
	while(step[*task_number] < task_data[*task_number].WCET){
		
		while(hook[*task_number] == 1){
			/*suspend operation*/
			//fprintf(stderr, "FLAG\n");
		}
		
		if(schedule[*task_number] == 1){

			/*Taskの処理*/
			calc();

			/*タスクデータの更新*/
			task_data[*task_number].Run_Time = task_data[*task_number].Run_Time + ET[*task_number];
			task_data[*task_number].Relative_Deadline = task_data[*task_number].Relative_Deadline - ET[*task_number];
			task_data[*task_number].Laxity_Time = task_data[*task_number].Relative_Deadline - task_data[*task_number].WCET + task_data[*task_number].Run_Time;
			task_data[*task_number].Laxity_Time += 0;
			//fprintf(stderr, "Laxity Time of Scheduled Task%d = %5.3lf\n", *task_number+1, task_data[*task_number].Laxity_Time);		
			
			fprintf(stderr, "Scheduled Task = %d\n", *task_number+1);	
			//fprintf(stderr, "%d\n", *task_number+1);
			
			/*最終ステップでの処理*/
			if(step[*task_number] == task_data[*task_number].WCET - 1){
				
				fprintf(stderr, "Task%d finishes\n", *task_number+1);
				pthread_mutex_lock(&mutex);
				finish[*task_number] = 1;
				pthread_mutex_unlock(&mutex);
	
				save_laxity[*task_number] = task_data[*task_number].Laxity_Time;
				save_laxity[*task_number] += 0;
		
				/*デッドラインミスしたタスクを測定*/
				if(task_data[*task_number].Laxity_Time < -0.0)
					deadline_miss_task[*task_number]++;
	
				if(deadline_miss_task[*task_number] > 0)
					fprintf(stderr, "task%d missed deadline\n", *task_number+1);
		
				pthread_mutex_lock(&mutex);
				state[*task_number] = 0;
				pthread_mutex_unlock(&mutex);
				task_data[*task_number].Laxity_Time = MAX;
				
			}
			
			pthread_mutex_lock(&mutex);
			step[*task_number]++;
			pthread_mutex_unlock(&mutex);
			
		}else{
			
			/*計算負荷*/
			load();
			
			/*タスクデータの更新*/
			//fprintf(stderr, "Non Scheduled Task%d\n", *task_number+1);	
			task_data[*task_number].Relative_Deadline = task_data[*task_number].Relative_Deadline - ET[*task_number];
			//fprintf(stderr, "Relative Deadline of Task%d = %5.3lf\n", *task_number+1, task_data[*task_number].Relative_Deadline);
			task_data[*task_number].Laxity_Time = task_data[*task_number].Relative_Deadline - task_data[*task_number].WCET + task_data[*task_number].Run_Time;
			task_data[*task_number].Laxity_Time += 0;
			//fprintf(stderr, "Laxity Time of Non Scheduled Task%d = %5.3lf\n", *task_number+1, task_data[*task_number].Laxity_Time);	
	
		}
		
         	pthread_mutex_lock(&mutex);
		schedule[*task_number] = 0;
		hook[*task_number] = 1;
        	pthread_mutex_unlock(&mutex);
	
	}
	
	pthread_mutex_lock(&mutex);
	rand_memory[*task_number][step[*task_number]] = 0;
	step[*task_number] = task_data[*task_number].WCET;
	hook[*task_number] = 0;
	pthread_mutex_unlock(&mutex);
	
	return 0;
}

/*LMCLF Schedular*/
void *thread_LMCLF(){
	
	int i,j;					//カウント用変数
	int hooker = 0;				//フックがかかっているタスク数
	int fin = -1;				//全処理が終了しているタスク数
	int total_memory;			//スケジュールされるタスクの総消費メモリ増分
	int count;					//起動しているタスクの数
	int sys_step = 0;			//全体のステップ
	
	pthread_t Task[TN];
	int ready[TN];				//スレッドを止めたら0
	
	
	fprintf(stderr,"Scheduler thread starts...\n");
	/*全タスクの処理が終了するまで繰り返す*/
	while(fin != TN)  {

		/*スケジュールに参加するタスクがあるか判定*/
		for(i=0;i<TN;i++){
		  //fprintf(stderr,"sys_step=%d,task_data[%d].Release_Time=%d\n",sys_step,i,task_data[i].Release_Time);
			if(sys_step == task_data[i].Release_Time){
			  fprintf(stderr,"Task %d/%d thread starts...\n",i+1,TN);
				pthread_create(&Task[i], NULL, thread_Tasks, &task_data[i].Number);
				pthread_mutex_lock(&mutex);
				state[i] = 1;
				pthread_mutex_unlock(&mutex);
				ready[i] = 1;

			}
		}
	
		/*全処理が終了しているタスク数の測定とスレッドの停止*/
		fin = 0;
		for(i=0;i<TN;i++){
			
			fin = fin + finish[i];
			
			/*スレッドの停止*/
			if(finish[i] == 1 && ready[i] == 1){
			  fprintf(stderr,"Task %d/%d thread stops...\n",i+1,TN);
				ready[i] = 0;
				pthread_join(Task[i],NULL);
			}
			
		}
		//fprintf(stderr, "fin = %d\n", fin);
		
		/*全タスクデータの更新が終わるまで待機*/
		count = -1;
		while(hooker != count){
			count = 0;
			for(i=0;i<TN;i++){
				if(state[i] == 1)
					count++;
			}
			
			hooker = 0;
			for(i=0;i<TN;i++){
				if(state[i] == 1){
					hooker = hooker + hook[i];
					//fprintf(stderr, "hooker[%d] = %d\n", i, hook[i]);
				}
				
			}
			//fprintf(stderr, "hooker = %d\n", hooker);
			//fprintf(stderr, "count = %d\n", count);

		}

		fprintf(stderr,"fin=%d,count=%d\n",fin,count);
		if(fin != TN && count > 0){
			
			LMCLF();			//優先度関数の小さい順にソート
			/*以下のコメントアウトを外すと出力が見れる*/		
		
			fprintf(stderr, "\n");
			for(j=0;j<TN;j++)
				fprintf(stderr, "Sort LMCLF[%d] = (%s * %d) + (%0.0lf * %0.0lf) = %s\n", sort_num_LMCLF[j]+1, FixPointDecimalToString(alpha), rand_memory[sort_num_LMCLF[j]][step[sort_num_LMCLF[j]]], task_data[sort_num_LMCLF[j]].WCET-step[sort_num_LMCLF[j]], task_data[sort_num_LMCLF[j]].Laxity_Time, FixPointDecimalToString(ITFD(sort_priority[j])));
			
			/*スケジュールされるタスクの選定*/
			for(i=0;i<P;i++){
			  if(state[sort_num_LMCLF[i]] == 1) {
				  pthread_mutex_lock(&mutex);
				  schedule[sort_num_LMCLF[i]] = 1;
				  pthread_mutex_unlock(&mutex);
			  }
			}

		}
		
		//fprintf(stderr, "FLAG\n");
		total_memory = 0;
		for(i=0;i<TN;i++){
			if(schedule[i] == 1)
				total_memory = total_memory + rand_memory[i][step[i]];
		}
		memory_recode(total_memory);				//メモリ消費量の記録
		
		sys_step++;				//全体のステップをインクリメント
		
		/*フックを外す(全タスク処理を再開)*/
		for(i=0;i<TN;i++) {
		  pthread_mutex_lock(&mutex);
		  hook[i] = 0;
		  pthread_mutex_unlock(&mutex);
		}
		
		hooker = 0;

	}
	
	return 0;
}


/*優先度関数値が小さい順にソートする関数*/
void LMCLF(){

	FIXPOINTDECIMAL priority_func[S];		//優先度関数値格納変数（作業用）
	FIXPOINTDECIMAL val=0,minval=MAX;               //評価値格納変数
	FIXPOINTDECIMAL priority_func1=0,priority_func2=0;
	int i = 0,j = 0,k = 0,l = 0,a = 0,b = 0,c = 0;		//カウント用変数
	int set1=0,set2=0; //1,2ステップ目のタスクの組み合わせ数
	FIXPOINTDECIMAL prealpha=alpha;	//前の周期のα
	FIXPOINTDECIMAL alphauppermin=MAX,alphalowermax=0;   //αの範囲最大最小
	FIXPOINTDECIMAL alphaupperminsav=MAX,alphalowermaxsav=0;   //αの範囲最大最小(保存用)
    FIXPOINTDECIMAL s1val=0,s2val=0;  //1ステップ目,2ステップ目の評価値の合計
	FIXPOINTDECIMAL tempalpha1=0,tempalpha2=0;  //仮のαの上限下限格納関数
	int valketa=0;	//評価値計算のための桁数を調整するためのもの
	int besti,bestk;   // 最小メモリとなるiとkを記憶
	int mintasknum=0;	//評価値が最小となるタスク番号
	int setP1num=0,setP2num=0;	//1,2ステップ目の評価値が低い上位のタスク番号の集合の要素数
	int Laxityjudge=MAX;	//余裕時間判定するための数
	int NewLaxityjudge=MAX;	//仮の余裕時間判定するための数

  	List setP1=createList();	//1ステップ目の評価値が低い上位のタスク番号の集合
	List setP2=createList();	//2ステップ目の評価値が低い上位のタスク番号の集合
	List val1=createList();		//1ステップ目の評価値集合
	List val2=createList();		//2ステップ目の評価値集合
	List copyval1=createList();		//1ステップ目の評価値集合
	List copyval2=createList();		//2ステップ目の評価値集合
  	List Aset1=createList();	//1ステップ目でのタスクの組み合わせの全体集合
	List Aset2=createList();    //２ステップ目でのタスクの組み合わせの全体集合
	List Aset1orig=createList();	//Aset1の先頭ポインタを記憶
	List Aset2orig=createList();	//Aset2の先頭ポインタを記憶
    List kumi1=createList();		//1ステップ目でのタスクの組み合わせ部分集合
    List kumi2=createList();		//2ステップ目でのタスクの組み合わせ部分集合
	List bestkumi1=createList();	//1ステップ目での最小メモリとなるタスクの組み合わせを記憶
	List bestkumi2=createList();	//2ステップ目での最小メモリとなるタスクの組み合わせを記憶
	
	pthread_mutex_lock(&mutex);
    alphadiff=0;
	minval=MAX;
	/*換算レートαの決定*/
  	for(i=TN-1;i>=0;i--){//val1に評価値を格納
	  if(state[i]==1){
		//メモリと時間の桁数を合わせることで仮のαを求めそれを用い評価値の計算
		//valketa=shiftoperationtimes((task_data[i].WCET - step[i]) * task_data[i].Laxity_Time) - shiftoperationtimes(rand_memory[i][step[i]]);
		//val1=insertList(((task_data[i].WCET - step[i]) * task_data[i].Laxity_Time) + ((valketa>=0)?(rand_memory[i][step[i]] << valketa):(rand_memory[i][step[i]] >> abs(valketa))),val1);
		val1=insertList((FTFD(task_data[i].WCET - step[i]) * FTFD(task_data[i].Laxity_Time)) + (prealpha * ITFD(rand_memory[i][step[i]])),val1);
	  }else{
		val1=insertList(MAX,val1);
	  }
  	}
	fprintf(stderr,"val1:"); fprintFPList(val1);
	
	for(i=0;i<valTN;i++){//評価値が小さいタスク番号を格納
		mintasknum=minList(val1,0,-1,MAX);
		if(mintasknum!=-1){
			setP1=insertList(mintasknum,setP1);
			copyval1=val1;
			val1=ideleatList(val1,copyval1,mintasknum);
		}
	}
	freeList(copyval1);

	setP1num=lengthList(setP1);

	fprintf(stderr,"step1探索範囲を狭めたタスク番号:"); fprintList(setP1);

	if(setP1num>=P){
  		Aset1=setList(setP1,createList(),0,setP1num-P+1);
	}else{
		Aset1=copyList(setP1);
	}

	fprintf(stderr,"step1タスク番号集合:"); fprintList(Aset1);
	freeList(setP1);

  	for(set1=0;set1<calcNumOfCombination(setP1num,P);set1++){
		if(!nullpList(kumi1)){
			freeList(kumi1);
		}

    	kumi1=firstnList(Aset1,P);
        Aset1=restnList(Aset1,P);
		//fprintf(stderr,"kumi1 %d組目:",set1); fprintList(kumi1);
	
		alphauppermin=MAX,alphalowermax=0;    
		for(i=0;i<TN;i++){
			if(memberList(i,kumi1)==1 && state[i] == 1){  /* iタスク目が選ばれていてかつタスクが起動していたら */
				for(j=0;j<TN;j++){
					if(memberList(j,kumi1)==0 && state[j] == 1){  //jタスク目が選ばれていないかつタスクが起動していたら
						if(rand_memory[i][step[i]] < rand_memory[j][step[j]]){// m(i)α+Ci*Li<m(j)α+Cj*Lj && m(j)>m(i) --> Ci*Li-Cj*Lj<(m(j)-m(i))α --> α>(Ci*Li-Cj*Lj)/(m(j)-m(i))
							tempalpha1=((FTFD(task_data[i].WCET - step[i]) * FTFD(task_data[i].Laxity_Time))-(FTFD(task_data[j].WCET - step[j]) * FTFD(task_data[j].Laxity_Time)))/(ITFD(rand_memory[j][step[j]])-ITFD(rand_memory[i][step[i]]));
							if(alphalowermax<tempalpha1){	
		    					alphalowermax=tempalpha1;
							}else{
							}
						}else{// m(i)α+Ci*Li<m(j)α+Cj*Lj && m(j)<m(i) --> (m(i)-m(j))α<Cj*Lj-Ci*Li --> α<(Cj*Lj-Ci*Li)/(m(i)-m(j))
							tempalpha2=((FTFD(task_data[j].WCET - step[j]) * FTFD(task_data[j].Laxity_Time))-(FTFD(task_data[i].WCET - step[i]) * FTFD(task_data[i].Laxity_Time)))/(ITFD(rand_memory[i][step[i]]) - ITFD(rand_memory[j][step[j]]));
							if(alphauppermin>tempalpha2){
		    					alphauppermin=tempalpha2;								
							}
						}
					}
				}
            }
        }
		
	    if(!(alphauppermin>0 && alphalowermax < alphauppermin)){
	    	continue;
	    }
		
	    //fprintf(stderr,"%lf <= alpha <= %lf for scheduling task %d first\n",alphalowermax,alphauppermin,i+1);
	    alphaupperminsav=alphauppermin; alphalowermaxsav=alphalowermax;

		//val2=createList();
		for(k=TN-1,val2=createList();k>=0;k--){//val2に評価値を格納
			if(state[k]==1){
				//メモリと時間の桁数を合わせることで仮のαを求めそれを用い評価値の計算
				//valketa=shiftoperationtimes((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k]) * task_data[k].Laxity_Time) - shiftoperationtimes(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]]);
				//val2=insertList(((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k]) * task_data[k].Laxity_Time) + (valketa>=0)?(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]] << valketa):(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]] >> abs(valketa)),val2);
				val2=insertList(((FTFD((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k])) * FTFD(task_data[k].Laxity_Time)) + (prealpha * ITFD(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]]))),val2);
			}else{
				val2=insertList(MAX,val2);
			}
		}

		fprintf(stderr,"val2:"); fprintFPList(val2);

		//setP2=createList();
		for(k=0,setP2=createList();k<valTN;k++){//評価値が小さいタスク番号を格納
			mintasknum=minList(val2,0,-1,MAX);
			if(mintasknum!=-1){
			setP2=insertList(mintasknum,setP2);
			copyval2=val2;
			val2=ideleatList(val2,copyval2,mintasknum);
			}
		}
		freeList(copyval2);

		setP2num=lengthList(setP2);
		fprintf(stderr,"step2探索範囲を狭めたタスク番号:"); fprintList(setP2);


		if(setP2num>=P){
			Aset2=setList(setP2,createList(),0,setP2num-P+1);
		}else{
			Aset2=copyList(setP2);
		}

		fprintf(stderr,"step2タスク番号集合:"); fprintList(Aset2);
		freeList(setP2);

		Aset2orig=Aset2;

  		for(set2=0;set2<calcNumOfCombination(setP2num,P);set2++){
			if(!nullpList(kumi2)){
			freeList(kumi2);
			}
            kumi2=firstnList(Aset2,P);
            Aset2=restnList(Aset2,P);

			alphauppermin=alphaupperminsav; alphalowermax=alphalowermaxsav;
	    	for(k=0;k<TN;k++){
	    		if(memberList(k,kumi2)==1 && state[k] == 1){ /* kタスク目が選ばれていてかつタスクが起動していたら */
					for(l=0;l<TN;l++){
						if(memberList(l,kumi2)==0 && state[l] == 1){  /* lタスク目が選ばれていないかつタスクが起動していたら */
		    				if(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]] < rand_memory[l][(memberList(l,kumi1)==1)?(step[l]+1):step[l]] ){  // m(k)α+Ck*Lk<m(l)α+Cl*Ll && m(l)>m(k) --> Ck*Lk-Cl*Ll<(m(l)-m(k))α --> α>(Ck*Lk-Cl*Ll)/(m(l)-m(k))
		    					tempalpha1=((FTFD((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k])) * FTFD(task_data[k].Laxity_Time))-(FTFD((task_data[l].WCET - (memberList(l,kumi1)==1)?(step[l]+1):step[l])) * FTFD(task_data[l].Laxity_Time)))/(ITFD(rand_memory[l][(memberList(l,kumi1)==1)?(step[l]+1):step[l]]) - ITFD(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]]));	
		    					if(alphalowermax<tempalpha1){
									alphalowermax=tempalpha1;
		    					}else{
		    					}
		    				}else{// m(k)α+Ck*Lk<m(l)α+Cl*Ll && m(l)<m(k) --> (m(k)-m(l))α<Cl*Ll-Ck*Lk  -->  α<(Cl*Ll-Ck*Lk)/(m(k)-m(l))
		    					tempalpha2=((FTFD((task_data[l].WCET - (memberList(l,kumi1)==1)?(step[l]+1):step[l])) * FTFD(task_data[l].Laxity_Time))-(FTFD((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k])) * FTFD(task_data[k].Laxity_Time)))/(ITFD(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]]) - ITFD(rand_memory[l][(memberList(l,kumi1)==1)?(step[l]+1):step[l]]));	
		    					if(alphauppermin>tempalpha2){
									alphauppermin=tempalpha2;
		    					}
                            }
						}
					}
                }
            }
			
            if(alphauppermin>0 && alphalowermax < alphauppermin){ //求めたいαが条件を満たしている時
                //fprintf(stderr,"%lf <= alpha <= %lf for scheduling task %d and then task %d\n",alphalowermax,alphauppermin,i+1,k+1);
				val=0;
				Laxityjudge=0;
                //1ステップ目の評価値の合計
                for(i=0,s1val=0;i<TN;i++){
                    if(memberList(i,kumi1)==1 && state[i] == 1){ /* set1のiビット目が1ならば */
						//valketa=shiftoperationtimes((task_data[i].WCET - step[i]) * task_data[i].Laxity_Time) - shiftoperationtimes(rand_memory[i][step[i]]);
                        //s1val+=(((task_data[i].WCET - step[i]) * task_data[i].Laxity_Time) + ((valketa>=0)?(rand_memory[i][step[i]] << valketa):(rand_memory[i][step[i]] >> abs(valketa))));
						s1val+=(FTFD((task_data[i].WCET - step[i])) * FTFD(task_data[i].Laxity_Time)) + (prealpha * ITFD(rand_memory[i][step[i]]));
					}
					if(task_data[i].Laxity_Time!=0){
						NewLaxityjudge=1/shiftoperationtimes((int)(task_data[i].Laxity_Time));
					}else{
						NewLaxityjudge=MAX;
					}
					if(NewLaxityjudge>Laxityjudge){
						Laxityjudge=NewLaxityjudge;
					}						
                }

                //2ステップ目の評価値の合計とLaxityjudgeの設定
                for(k=0,s2val=0;k<TN;k++){
                    if(memberList(k,kumi2)==1 && state[k] == 1){ /* set2のiビット目が1ならばs2valに評価値を格納 */
						//valketa=shiftoperationtimes((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k]) * task_data[k].Laxity_Time) - shiftoperationtimes(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]]);
                        //s2val+=((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k]) * task_data[k].Laxity_Time) + (valketa>=0)?(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]] << valketa):(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]] >> abs(valketa));
						s2val+=((FTFD((task_data[k].WCET - (memberList(k,kumi1)==1)?(step[k]+1):step[k])) * FTFD(task_data[k].Laxity_Time)) + (prealpha * ITFD(rand_memory[k][(memberList(k,kumi1)==1)?(step[k]+1):step[k]])));
						
					}
					if(task_data[k].Laxity_Time!=0){//0になるまでシフト演算してその回数が少ないほどLaxityjudgeを大きくして逆の場合は小さくしたい
						NewLaxityjudge=1/shiftoperationtimes((int)(task_data[k].Laxity_Time));
					}else{
						NewLaxityjudge=MAX;
					}

					if(NewLaxityjudge>Laxityjudge){
						Laxityjudge=NewLaxityjudge;
					}
                }

                if(s1val>s1val+s2val){  //1ステップ目の評価値の合計が1ステップ目,2ステップ目の評価値の合計より大きい場合
                    val=s1val;
                }else{  //小さい場合
                    val=s1val+s2val;
                }
				
              	if(minval>val){  //今まで求めた最小の評価値よりも小さいとき
		    		minval=val;  bestkumi1=copyList(kumi1); bestkumi2=copyList(kumi2);
	    			if(alphauppermin<MAX){//αの下限の最大値がMAXより小さい場合αの下限の最大値と上限の最小値を足して2で割った値をαの候補とし、さらに余裕時間の大きさによってαの値を調整
						alpha=((alphalowermax + alphauppermin)/2) >> Laxityjudge;
					}else if(alphauppermin>=MAX){//さもなければαの下限の最大値をαの候補とし、さらに余裕時間の大きさによってαの値を調整
						alpha=(alphalowermax) >> Laxityjudge;
					}
				}
			}			
		}
		freeList(Aset2orig);
   	}
	freeList(Aset1orig);	
		
		
	
	
	fprintf(stderr,"prealpha=%s,newalpha=%s\n",FixPointDecimalToString(prealpha),FixPointDecimalToString(alpha));
	fprintf(stderr,"step1:"); fprintList(bestkumi1);
	fprintf(stderr,"step2:"); fprintList(bestkumi2);
	fprintf(stderr,"minval=%s \n",FixPointDecimalToString(minval));
	fprintf(stderr,"Laxityjudge=%d \n",Laxityjudge);
	pthread_mutex_unlock(&mutex);

		

	/*優先度関数値の初期化*/
	for(i=0;i<TN;i++){
		if(state[i] == 1 && step[i] != task_data[i].WCET){

			//priority_func[i] = (alpha * rand_memory[i][step[i]]) + ((task_data[i].WCET - step[i]) * task_data[i].Laxity_Time);		
			priority_func[i] = (alpha * ITFD(rand_memory[i][step[i]])) + (FTFD(task_data[i].WCET - step[i]) * FTFD(task_data[i].Laxity_Time));		

		}else{
			priority_func[i] = MAX;
		}
	}

	/*ソート用変数初期化*/
	for(k=0;k<TN;k++){
		sort_priority[k] = priority_func[k];
		sort_num_LMCLF[k] = k;
		//fprintf(stderr, "sort_priority[%d] = %lf\n", k, sort_priority[k]);
	}
	
	int t1 = 0, t2 = 0;
	/*優先度関数値が小さい順にソートする.*/
	for(j=0;j<TN-1;j++){
		for(k=TN-1;k>j;k--){
			if(sort_priority[k] < sort_priority[k-1]){
				t1 = sort_priority[k];
				sort_priority[k] = sort_priority[k-1];
				sort_priority[k-1] = t1;
				
				t2 = sort_num_LMCLF[k];
				sort_num_LMCLF[k] = sort_num_LMCLF[k-1];
				sort_num_LMCLF[k-1] = t2;
			}
		}
	}
	
}


/*最悪メモリ消費量を記憶する関数*/
void memory_recode(int Memory){
	
	Current_Memory = Current_Memory + Memory;	//現在のメモリ消費量
	fprintf(stderr, "Current_Memory = %d\n", Current_Memory);
	
	if(Worst_Memory < Current_Memory){
		Worst_Memory = Current_Memory;
		fprintf(stderr, "Worst_Memory = %d\n", Worst_Memory);
	}
	
}


/*Taskの処理*/

void calc(){
	int n,j;
	n = 1;
	for(j=1;j<1000000;j++){
		n *= 1;
	}
}


/*計算負荷*/

void load(){
	int n,j;
	n = 1;
	for(j=1;j<1000000;j++){
		n *= 1;
	}
}

//桁数を数える
int get_digit(int n){
	int digit=1;

	while(n/=10){
		digit++;
	}

	return digit;
}

//空のリストを生成
List createList(void){
  	return NIL;
}

//リストの先頭に要素を入れる
List insertList(FIXPOINTDECIMAL element,List l) {
	List temp=malloc(sizeof(Cell));
  	temp->element=element;
	  	//fprintf(stderr,"temp->element:%lld ",temp->element); 
  	temp->next=l;
	  	//fprintf(stderr,"temp:"); fprintList(temp);
  	return temp;
}

//
int nullpList(List l) {
  	return (l==NIL);
}

//リストの先頭要素を返す
int headList(List l) {
  	return l->element;
}

//先頭要素を削除する
List tailList(List l) {
  	return l->next;
}


int tailarray(List l,int i){
  	return i++;
}


List appendList(List l1,List l2){

 	if (nullpList(l1)){
    	return l2;
  	}else{
    	return insertList(headList(l1),appendList(tailList(l1),l2));

  	}
}

//先頭からi番目の要素を返す
int iList(List l1,int i){

  	if(i==0){
    	return headList(l1);
  	}else{
    	return iList(tailList(l1),i-1);
  	}

}

//先頭からi番目の要素を書き換える
List ideleatList(List l1,List l2,int i){
  	if(i==0){
		l1->element=MAX;
    	return l2;
  	}else{
    	return ideleatList(tailList(l1),l2,i-1);
  	}

}
//リストの全要素を表示
void printList(List l) {

  	if(nullpList(l)){ 
    	printf("\n"); 
  	}else{
    	printf("%lld ",headList(l));
    	printList(tailList(l));
  	}
}

//リストの全要素をファイル出力
void fprintList(List l) {

  	if(nullpList(l)){ 
    	fprintf(stderr, "\n"); 
  	}else{
    	fprintf(stderr, "%lld ", headList(l));
    	fprintList(tailList(l));
  	}
}

void fprintFPList(List l) {

  	if(nullpList(l)){ 
    	fprintf(stderr, "\n"); 
  	}else{
    	fprintf(stderr, "%s ", FixPointDecimalToString(headList(l)));
    	fprintList(tailList(l));
  	}
}

//先頭からn番目までの要素をリストに入れて返す
List firstnList(List l,unsigned int n){

  	if(nullpList(l) || n==0){  //lが空でないまたはnが0のとき空のリストを返す
    	return createList();
  	}else{  //それ以外の時lの先頭要素をfirstnListに書き込む
    	return insertList(headList(l),firstnList(tailList(l),--n));
  	}
}

//先頭からn番目までの要素を削除
List restnList(List l,unsigned int n){
  	if(nullpList(l) || n==0){  //lが空でないまたはnが0のとき空のリストを返す
    	return l;
  	}else{  //それ以外の時先頭の要素を取り除く
    	restnList(tailList(l),--n);
  	}
}

//リストに要素が含まれているかどうかを探索
int memberList(int element,List l){

  if(nullpList(l)){
    return 0;
  }else if(element==headList(l)){
    return 1;
  }else{
    return memberList(element,tailList(l));
  }

}

//リストのメモリ解放
void freeList(List l){
	if(nullpList(l)){
		return;
	}else{
		freeList(tailList(l));
		free(l);
		return;
	}

}

//リストの総数を数える
int lengthList(List l){

  if(nullpList(l)){
    return 0;
  }else{
    return lengthList(tailList(l))+1;
  }
}

//リストの複製を作成
List copyList(List l){

	return firstnList(l,lengthList(l));

}

//リストの評価値が最小であるタスク番号を返す関数
int minList(List l,int tasknum1,int tasknum2,FIXPOINTDECIMAL min){
	if(nullpList(l)){
		return tasknum2;
	}else if(headList(l)<min){
		min=headList(l);
		tasknum2=tasknum1;
		return minList(tailList(l),++tasknum1,tasknum2,min);
	}else{
		return minList(tailList(l),++tasknum1,tasknum2,min);
	}
}

//選ばれるタスクの選定
List setList(List sourceList,List subsetList,int begin,int end){
  	List p=createList();
	List oldp=createList();
  	List temp=createList();
  	int i=0;

  	for(i=begin;i<end;i++){
    	temp=copyList(appendList(subsetList,insertList(iList(sourceList,i),createList())));
    	if(end+1<=lengthList(sourceList)){
			oldp=p;
      		p=copyList(appendList(p,setList(sourceList,temp,i+1,end+1)));
			freeList(oldp);
			freeList(temp);
    	}else{
			oldp=p;
      		p=copyList(appendList(p,temp));
			freeList(oldp);
			freeList(temp);
    	}
  	}
  	return p;
}

//組み合わせの総数を求める
int calcNumOfCombination(int n, int r){
    int num = 1;
	
	if(n<=r){
		return 1;
	}else{
		for(int i = 1; i <= r; i++){
        	num = num * (n - i + 1) / i;
		}
		return num;
    }
}

//シフト演算によりシフトする回数を取得
int shiftoperationtimes(FIXPOINTDECIMAL n){
	
	if(n==0 || n==-1){
		return 0;
	}else{
		return 1 + shiftoperationtimes(n >> 1);
	}
}

FIXPOINTDECIMAL FTFD(double x){
	return (FIXPOINTDECIMAL)(x*pow(2,FIXEDPOINT));
}

char *FixPointDecimalToString(FIXPOINTDECIMAL x){
	int ipart=0,fpart=0;
	char buf[S];

	ipart= x >>FIXEDPOINT;
	fpart= x - (ipart << FIXEDPOINT);

	snprintf(buf,sizeof(buf),"%d.%0d",ipart,fpart);

	return strdup(buf);
}
