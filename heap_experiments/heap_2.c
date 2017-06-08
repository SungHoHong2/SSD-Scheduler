#include <stdlib.h>
#include <stdio.h>

struct request{
    int proof;
};

struct sfq_request{
  int start_tag;
  struct request *rq;
};


int main(){

  // pointer array
  sfq_request *sfq_1 = (sfq_request *)malloc(sizeof(sfq_request));
               sfq_1->rq = (request *)malloc(sizeof(request));
               sfq_1->rq->proof =1;

  sfq_request *sfq_2 = (sfq_request *)malloc(sizeof(sfq_request));
               sfq_2->rq = (request *)malloc(sizeof(request));
               sfq_2->rq->proof =0;

  sfq_request *sfq_3 = (sfq_request *)malloc(sizeof(sfq_request));
               sfq_3->rq = (request *)malloc(sizeof(request));
               sfq_3->rq->proof =1;

  sfq_request *sfq_4 = (sfq_request *)malloc(sizeof(sfq_request));
               sfq_4->rq = (request *)malloc(sizeof(request));
               sfq_4->rq->proof =0;

   sfq_request *sfq_5 = (sfq_request *)malloc(sizeof(sfq_request));
                sfq_5->rq = (request *)malloc(sizeof(request));
                sfq_5->rq->proof =1;

   sfq_request *sfq_6 = (sfq_request *)malloc(sizeof(sfq_request));
                sfq_6->rq = (request *)malloc(sizeof(request));
                sfq_6->rq->proof =0;

   sfq_request *sfq_7 = (sfq_request *)malloc(sizeof(sfq_request));
                sfq_7->rq = (request *)malloc(sizeof(request));
                sfq_7->rq->proof =1;

   sfq_request *sfq_8 = (sfq_request *)malloc(sizeof(sfq_request));
                sfq_8->rq = (request *)malloc(sizeof(request));
                sfq_8->rq->proof =0;

   sfq_request *sfq_9 = (sfq_request *)malloc(sizeof(sfq_request));
                sfq_9->rq = (request *)malloc(sizeof(request));
                sfq_9->rq->proof =0;

  sfq_request *sfq_10 = (sfq_request *)malloc(sizeof(sfq_request));
               sfq_10->rq = (request *)malloc(sizeof(request));
               sfq_10->rq->proof =0;

   sfq_request *sfq_11 = (sfq_request *)malloc(sizeof(sfq_request));
                sfq_11->rq = (request *)malloc(sizeof(request));
                sfq_11->rq->proof =0;


  sfq_1->start_tag = 1;
  sfq_2->start_tag = 2;
  sfq_3->start_tag = 3;
  sfq_4->start_tag = 4;
  sfq_5->start_tag = 5;
  sfq_6->start_tag = 6;
  sfq_7->start_tag = 7;
  sfq_8->start_tag = 8;
  sfq_9->start_tag = 9;
  sfq_10->start_tag = 10;
  sfq_11->start_tag = 0;


  sfq_request **sfqqs = (sfq_request **)malloc(10*sizeof(sfq_request *));
  sfqqs[0] = sfq_1;
  sfqqs[1] = sfq_2;
  sfqqs[2] = sfq_3;
  sfqqs[3] = sfq_4;
  sfqqs[4] = sfq_5;
  sfqqs[5] = sfq_6;
  sfqqs[6] = sfq_7;
  sfqqs[7] = sfq_8;
  sfqqs[8] = sfq_9;
  sfqqs[9] = sfq_10;

  for(int i=0; i<10; i++)
  printf("%d %d\n", sfqqs[i]->start_tag, sfqqs[i]->rq->proof);

  sfq_request *temp;
  int no, i, j, c, root;
  no = 10;

  for(i=0; i<no; i++){
    c = i;
    do{
        root = (c-1)/2;
        if(sfqqs[root]->start_tag < sfqqs[c]->start_tag){
            temp = sfqqs[root];
            sfqqs[root] = sfqqs[c];
            sfqqs[c] = temp;
        }
        c = root;
    } while( c!= 0 );
  }

  printf("Heap array : ");
  for(i=0; i<no; i++)
    printf("%d\t", sfqqs[i]->start_tag);

//done

  for(j=no-1; j>=0; j--){
      temp = sfqqs[0];
      sfqqs[0] = sfqqs[j];
      sfqqs[j] = temp;
      root = 0;


      do {
            c = 2 * root + 1;
            printf("while working: %d\n", c);
            if((sfqqs[c]->start_tag < sfqqs[c+1]->start_tag) && c <j-1)
              c++;
            if(sfqqs[root]->start_tag < sfqqs[c]->start_tag && c<j){
              temp = sfqqs[root];
              sfqqs[root] = sfqqs[c];
              sfqqs[c] = temp;
            }
            root = c;
      } while(c < j);

  }

  printf("\n the sorted array is : ");
  for(i=0; i<no; i++)
    printf("\t %d", sfqqs[i]->start_tag);

  return 0;

}
