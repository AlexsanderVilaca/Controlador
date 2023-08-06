#include <avr/io.h>
#include <avr/interrupt.h>

#define ALE 0x01
#define WRITE 0x02
#define READ 0x04

#define SOBE		1
#define DESCE		2
#define START		3
#define ENTER		4

char* MenuPrincipal[2] = { "ctrl", "back" };
char* MenuControle[4] = { "sp.h", "sp.l", "time", "back" };
char* MenuSetpoint[3] = { "sp", "hyst", "back." };
char* MenuSimNao[2] = { "yes", "no" };
char* MenuReles[3] = { "rly.1", "rly.2", "rly.3" };
char* MenuDirOrRevAction = { "dir", "rev" };
	
char Setpoint[2] = { 0, 0 }; /* H, L */
char ControlTime = 0;
char Hysteresis = 5;
char HeatingControl = 0;
char IsActionNA = 0;

unsigned int ContadorScan = 0;
char EstadoControle = 0;

char	valor_display[4] = { 0x00, 0x00, 0x00, 0x00 };
char	end_display[4] = { 0x01, 0x02, 0x04, 0x08 }; /* endereco dos displays */
char	indice = 0;

int global = 0;
char counter;

/* °C/uV */
float coeficients_TC_J[35] =
{
	2.1276596e-2, 2.0876827e-2, 2.0533881e-2, 2.0242915e-2, 1.9960080e-2,
	1.9723866e-2, 1.9531250e-2, 1.9305019e-2, 1.9157088e-2, 1.9011407e-2,
	1.8832392e-2, 1.8726592e-2, 1.8621974e-2, 1.8552876e-2, 1.8416206e-2,
	1.8348624e-2, 1.8315018e-2, 1.8214936e-2, 1.8181818e-2, 1.8148820e-2,
	1.8115942e-2, 1.8083183e-2, 1.8050542e-2, 1.8018018e-2, 1.8018018e-2,
	1.8018018e-2, 1.8018018e-2, 1.7985612e-2, 1.8018018e-2, 1.8018018e-2,
	1.8018018e-2, 1.8018018e-2, 1.8050542e-2, 1.8050542e-2, 1.8050542e-2
};

int voltage_TC_J[36] = /* voltages in uV */
{
	/* -50°C */
	-2431, -1961, -1482, -995, -501,
	0, 507, 1019, 1537, 2059,
	2585, 3116, 3650, 4187, 4726,
	5269, 5814, 6360, 6909, 7459,
	8010, 8562, 9115, 9669, 10224,
	10779, 11334, 11889, 12445, 13000,
	13555, 14110, 14665, 15219, 15773,
	16327
	/* 300°C */
};

char	tabela_digitos[10] = 
{ /* 0		1	 2	   3	 4 */
	0x3F, 0x06, 0x5B, 0x4F, 0x66,
	/* 5	6	 7	   8	 9	*/
	0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

char	tabela_letras[] =
{
	/* a,   b,   c,   d,   e,   f,   g,   h,*/
	0x77,0x7C,0x39,0x5E,0x79,0x71,0x7D,0x74,
	/* i,   j,   k,   l,   m,   n,   o,   p,*/
	0x06,0x1E,0x72,0x38,0x54,0x54,0x5C,0x73,
	/* q,   r,   s,   t,   u,   v,   w,   x,*/
	0x67,0x50,0x6D,0x78,0xC1,0x3E,0x00,0x00,
	/* y,   z,*/
	0x6E,0x00
};

ISR(TIMER0_OVF_vect)
{
	TIMSK0 &= ~(0x01);
	
	ContadorScan++; /* conta a cada 2ms */
	if (ContadorScan == 30) /* 60ms */
	{
		ContadorScan = 0;
	}
	TCNT0 = 0xF0; /* 5ms */ 
	
	for(counter = 0; counter < 0xF0; counter++);
	PORTC |= 0x01; /* ALE = 1 */
	PORTD = 0x00;
	PORTC &= ~(0x01); /* ALE = 0 */
	PORTC |= 0x01; /* ALE = 1 */
	PORTD = valor_display[indice];
	PORTC |= 0x02; /* grava no 373 de dados */
	PORTC &= ~(0x02); /* grava no 373 de dados */
	PORTD = 0x01;
	PORTC &= ~(0x01);
	PORTC |= 0x01;
	PORTD = end_display[indice];
	PORTC |= 0x02;
	PORTC &= ~(0x02);
	for(counter = 0; counter < 0xF0; counter++);
	/* apaga todos os diplays */
	PORTD = 0x01;
	PORTC &= ~(0x01);
	PORTC |= 0x01;
	PORTD = 0x00;
	PORTC |= 0x02;
	PORTC &= ~(0x02);
	  
	indice++; /* muda para o próximo display */
	if (indice == 4)
		indice = 0;
		
	TIMSK0 |= 0x01;
}

int TC (int* mv) /* retorna o limInferior com base no intervalo aberto [limInferior, limSuperior[ */
{
	int valor = *mv;
	
	char limSuperior = 35,
	limInferior = 0,
	meio;
	
	while (1)
	{
		meio = (limInferior + limSuperior) / 2;
		if (voltage_TC_J[meio] > valor) /* segunda metade do conjunto */
		{
			limSuperior = meio;
		}
		else /* primeira metado do conjunto */
		{
			limInferior = meio;
		}
		if ((limSuperior - limInferior) == 1) /* enquanto não forem pontos consecutivos */
			return ((coeficients_TC_J[limInferior]*(valor-voltage_TC_J[limInferior])*100 
					+  (limInferior-5)*1000)*0.1);
	}
}

char LeTeclado()
{	
	char leitura = 0;
	char i = 0;
	
	TIMSK0 &= ~(0x01);

	DDRD = 0x00; /* define PORTD como entradas */
	
	PORTC &= ~(0x04); /* READ = 0 */
	leitura = PIND;
	PORTC |= 0x04; /* READ = 1 */
	DDRD = 0xFF;
	
	TIMSK0 |= 0x01;
	
	for (i = 0; i < 4; i++)
	{
		if (leitura & (1 << i))
		{
			return ( i + 1 );
		}
	}
		
	return ( 0 );
}

void MostraInteiro(int* value, char casas)
{
	int number = *value;
	char vetor[4];
	char i = 0;
	char negativo = 0;
	
	valor_display[0] = 0x00;
	valor_display[1] = 0x00;
	valor_display[2] = 0x00;
	valor_display[3] = 0x00;
	
	if (number > 9999 || number < -999)
	{
		if (number > 9999)
		{
			valor_display[3] = tabela_letras['r' - 'a'];
			valor_display[2] = tabela_letras['d' - 'a'];
			valor_display[1] = tabela_letras['n' - 'a'];
			valor_display[0] = tabela_letras['u' - 'a'];
		}
		else
		{
			valor_display[3] = tabela_letras['r' - 'a'];
			valor_display[2] = tabela_letras['e' - 'a'];
			valor_display[1] = tabela_letras['v' - 'a'];
			valor_display[0] = tabela_letras['o' - 'a'];
		}
		return;
	}
	
	if (number < 0)
	{
		negativo = 1;
		number = -number;
	}
	for (i = 3; i >= 0; i--)
	{
		if (number == 0)
		{
			if (i == 3)
			{
				valor_display[3] = tabela_digitos[0];
			}
			break;
		}
		vetor[i] = number % 10;
		number /= 10;
		valor_display[i] = tabela_digitos[vetor[i]];
	}
	if (negativo)
	{
		valor_display[i] = 0x40;
	}
	
	if (casas > 0 && casas <= 3)
	{
		valor_display[3-casas] |= 0x80;
	}
}

void MostraString(char* texto)
{
	char letra = *texto;
	char i = 0;
	
	valor_display[0] = 0x00;
	valor_display[1] = 0x00;
	valor_display[2] = 0x00;	
	valor_display[3] = 0x00;
	
	for (; i < 4; i++) /* num de displays */
	{
		letra = *texto;	
		if (letra)
		{
			if (letra >= 'a' && letra <= 'z')
			{
				valor_display[i] = tabela_letras[letra - 'a'];
			}
			else
			if (letra >= '0' && letra <= '9')
			{
				valor_display[i] = tabela_digitos[letra - '0'];
			}
			else
			if (letra == '.')
			{
				valor_display[--i] |= 0x80;
			}
			texto++;
		}
	}
	if (*texto == '.') /* apenas para o ponto decimal do quarto display */
	{
		valor_display[3] |= 0x80;
	}
}

void Menu(char** text, char* option, char lenght)
{
	char	tecla = 0,
			op = 0;
	
	while(1)
	{	
		tecla = LeTeclado();
		while(tecla == LeTeclado());
		
		if(tecla == SOBE)
		{
			op++;
			if (op == lenght)
			{
				op = 0;
			}
		}
		else
		if (tecla == DESCE)
		{
			op--;
			if (op == 255)
			{
				op = lenght-1;
			}
		}
		else
		if (tecla == ENTER)
		{
			*option = op;
			return;
		}
		MostraString(*(text+op));
	}
}

void ConfigMode()
{
	char opEscolhida[4] = { 0, 0, 0, 0 };
	
	while(1)
	{
		Menu(&MenuPrincipal, &opEscolhida[0], 2);
		if (opEscolhida[0] == 0)
		{
			while (1)
			{
				Menu(&MenuControle, &opEscolhida[1], 4);
				if(opEscolhida[1] < 2)
				{
					Menu(&MenuSetpoint, &opEscolhida[2], 3);
					if(opEscolhida[2] == 0) /* sp */
					{
						Menu(&MenuSimNao, &opEscolhida[3], 2);
					}
					else
					if (opEscolhida[2] == 1) /* hyst */
					{
						break;	
					}
					else
					if(opEscolhida[2] == 2) /* back */
					{
						break;
					}
				}
				else
				if (opEscolhida[1] == 3)
				{
					break;
				}
			}
		}
		else
		if (opEscolhida[0] == 1)
		{
			break;
		}
	}
}

void ADC_Init()
{
	DDRC=0x7F;			/* Make ADC port as input */
	ADCSRA = 0x87;			/* Enable ADC, fr/128  */
	ADMUX = 0x07;			/* Vref: Avcc, ADC channel: 0 */
}

int ADC_Read(char channel)
{
	int Ain,AinLow;
	
	ADMUX=ADMUX|(channel & 0x0f);	/* Set input channel to read */

	ADCSRA |= (1<<ADSC);		/* Inicia conversão */
	while((ADCSRA&(1<<ADIF))==0);	/* Aguarda o término da conversão */
	
	AinLow = (int)ADCL;		/* Lê o LSB */
	Ain = (int)ADCH*256;	/* Lê o MSB */ 

	Ain = Ain + AinLow;				
	return (Ain);
}

void Operacao()
{
	int leitura =  ADC_Read(7);
	
	if (EstadoControle == 1)
	{
		if (leitura >= (Setpoint + Hysteresis))
		{
			HeatingControl = 0;
		}
		else
		if (leitura <= (Setpoint - Hysteresis))
		{
			HeatingControl = 1;
		}
	}
	else
	if (EstadoControle == 2)
	{
		HeatingControl = 0;
	}
	
}

int main(void)
{
	DDRD = 0xFF; /* define o PORTD como saída */
	DDRC = 0xFF; /* define o PORTC como saída */
	DDRB = 0x00; /* define o PORTB como entrada */
	PORTB = 0x00;
	TCCR0B = 0x02; /* prescaler of 256 */
	TCNT0 = 0xD0; /* 2ms */
	TIMSK0 = 0x01; /* seta apenas o TOEI0 (timer overflow enable interrupt) */
	sei(); // set global interrupts
	PORTC = 0x04;
	
	ADC_Init();
	
	char estado_menu = 0;
	
	char	tecla = 0, 
			tecla_ant = 0;
	char option = 0;
	
	int a = -5;
	Operacao();
	
	//MostraInteiro((int*)&a);
	while (1)
	{
		//a = ADC_Read(7);
		//a = TC ((int*)&a);
		//a *= (20000./1023);

		//for (char i = 0; i < 0xFF; i++);
		
		tecla_ant = tecla;
		tecla = LeTeclado();
		
		if ((tecla == tecla_ant) && tecla != 0) /* tecla pressionada por dois scans consecutivos */
		{
			if (tecla == SOBE)
			{
				estado_menu++;
				if (estado_menu == 2)
				estado_menu = 0;
			}
			else
			if (tecla == DESCE)
			{
				estado_menu--;
				if (estado_menu == 255)
				estado_menu = 1;
			}
		}
		
		
		if (estado_menu == 0)
		{
			MostraString("conf");
			
			if (LeTeclado() == ENTER)
			{
				ConfigMode();
			}
			
		}
		else
		if (estado_menu == 1)
		{
			MostraString("ch.1");
		}
		
		//for (char i = 0; i < 0xFF; i++);
	}
}