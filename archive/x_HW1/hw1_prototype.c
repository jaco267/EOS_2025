#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <fcntl.h>
#define MAX_ITEMS 2   
#define MAX_NAME_LEN 30 

// struct for one item (name + price)
typedef struct {
  char name[MAX_NAME_LEN]; 
  int price;   
} Item;
typedef struct {
  char name[MAX_NAME_LEN]; 
  Item items[MAX_ITEMS]; 
  int itemCount;   
  int distance;  
} Shop; 
Shop shops[] = {
    {"Dessert shop",
     {{"cookie",60},
      {"cake",  80}},
      2,
      3
    },
    {"Beverage shop",
     {{"tea", 40},
      {"boba",70}},
      2,
      5
    },
    {"Diner",
     {{"fried rice", 120},
      {"egg-drop soup", 50}},
      2,
      8
    }   
};
int shopCount = 3;

int printHomePage(){
    printf("1. shop list \n");
    printf("2. order \n");  
    int inOpt; 
    scanf("%d", &inOpt ); 
    fgetc(stdin);  //flush the \n input by user  
    printf("input %d\n",inOpt);
    return inOpt;
}
void showShopListPage(){
  printf("Dessert shop: 3km\n");
  printf("Beverage shop: 5km\n");
  printf("Diner: 8km\n");
  int input; 
  scanf("%d", &input ); 
  fgetc(stdin); 
  /// 按任意鍵回主選單 
}
void shopPage(int shopOpt){
  if(!(shopOpt<=3 && shopOpt>=1)){return;}
  int order_flag = 0;
  int total_price = 0;
  while (1){
    //訂餐選單
        // Print the data
    for (int i = 0; i < shopCount; i++) {
        printf("%s -\n", shops[i].name);
        for (int j = 0; j < shops[i].itemCount; j++) {
            printf("    %s: $%d\n", shops[i].items[j].name, shops[i].items[j].price);
        }
        printf("\n");
    }
    printf("Please choose from 1~4\n");
    printf("1. %s: $%d\n",shops[shopOpt-1].items[0].name, shops[shopOpt-1].items[0].price);
    printf("2. %s: $%d\n",shops[shopOpt-1].items[1].name, shops[shopOpt-1].items[1].price);
    printf("3. confirm\n");
    printf("4. cancel\n");
    int order_input; 
    scanf("%d", &order_input ); 
    fgetc(stdin); 
    int order_number = -1;
        
    if ((order_input == 1) || (order_input == 2)){
        int price = shops[shopOpt-1].items[order_input-1].price;
        printf("How many?\n"); 
        scanf("%d", &order_number ); 
        fgetc(stdin);
        total_price += order_number*price; 
        order_flag = 1;
    }else if (order_input == 3){
        if(order_flag == 0){
            printf("havent order yet\n"); 
            //back to 訂餐選單
        }else{
            printf("Please wait for a few minutes...\n");
            printf("Please pick up your meal...\n");
            printf("total price = %d\nDistance = %d\n",total_price,shops[shopOpt-1].distance);
            int fd = open("/dev/etx_device", O_WRONLY);
            if (fd < 0) {perror("Failed to open /dev/etx_device");return;}
            char total_price_str[MAX_NAME_LEN];  
            char distance_str[MAX_NAME_LEN]; 
            //* write price  
            snprintf(total_price_str, sizeof(total_price_str), "7seg %d", total_price);
            ssize_t ret = write(fd, total_price_str, strlen(total_price_str));
            if (ret < 0) {perror("Failed to write to the device");close(fd);return;}

            int tmp_dist = shops[shopOpt-1].distance;
            for(int tt = tmp_dist; tt>=0; tt--){
              snprintf(distance_str, sizeof(distance_str), "led %d", tt);
              ret = write(fd, distance_str, strlen(distance_str));
              if (ret < 0) {perror("Failed to write to the device");close(fd);return;}
              usleep(1000000);  // 延遲 1 秒再顯示下一個
            }
            close(fd);
            /*
            show total price on 7seg  
            echo "7seg 3050" > /dev/etx_device

            /// turn off all led  
            echo "led 0" > /dev/etx_device 
            echo "led 1" > /dev/etx_device  //led1 on   
            ...
            echo "led 8" > /dev/etx_device  //led8 on  
            */
            int tmp_opt; 
            scanf("%d", &tmp_opt ); 
            fgetc(stdin);
            
            break;  //back to 主選單   
        }
    }else if (order_input == 4){
        printf("cancel\n");
        break;
    }else{
        printf("order should be from 1~4\n"); 
        break;
    }
  }

}

int main(int argc, char* argv[]){

  while(1){
    int menuOpt = printHomePage();//1. shoplist, 2. order
    //*---user input--
    if(menuOpt == 1){
      showShopListPage();
    }else if(menuOpt == 2){
      printf("Please choose from 1~3\n");
      for(int i =0; i<shopCount; i++){
        /*0. Dessert shop
          1. Beverage shop
          2. Diner
        */
        printf("%d. %s\n",i+1,shops[i].name); //Dessert shop
      }
      int shopOpt;
      scanf("%d", &shopOpt );
      fgetc(stdin);  
      if (shopOpt<=3 && shopOpt>=1){
        shopPage(shopOpt);
      }else{
        printf("shop option only from 1~3\n");
        break;
      }
    }else{
        printf("menu option only 1,2\n");
        break;
    }
  }

  return 0;
}