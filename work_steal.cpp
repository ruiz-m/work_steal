#include <iostream>
#include <omp.h>
#include <deque>
#include <vector>

enum struct Exp_cl
{
	Bool,
	Int,
	Plus
};

class Exp
{
	private:
		Exp_cl cl;

	public:
		Exp(Exp_cl cl) : cl(cl) {}
		Exp_cl getCl();
		void printCl();
		virtual ~Exp() {}
};

Exp_cl Exp::getCl()
{
	return cl;
}

void Exp::printCl()
{
	switch(cl)
	{
		case Exp_cl::Bool:
			std::cout << "Bool\n";
			break;
		case Exp_cl::Int:
			std::cout << "Int\n";
			break;
		case Exp_cl::Plus:
			std::cout << "Plus\n";
			break;
		default:
			std::cout << "Error\n";
			exit(1);
	}
}

class Plus : public Exp
{
	private:
		Exp *e1;
		Exp *e2;

	public:
		Plus(Exp *exp1, Exp *exp2) : Exp(Exp_cl::Plus), e1(exp1), e2(exp2) {}
		~Plus()
		{
			std::cout << "Deleting plus\n";
			delete e1;
			delete e2;
		}
		Exp *getE1();
		Exp *getE2();
};

Exp *Plus::getE1()
{
	return e1;
}

Exp *Plus::getE2()
{
	return e2;
}

enum struct Typ
{
	None,
	Bool,
	Int
};

void printTyp(Typ ty)
{
	switch(ty)
	{
		case Typ::None:
			std::cout << "None\n";
			break;
		case Typ::Bool:
			std::cout << "Bool\n";
			break;
		case Typ::Int:
			std::cout << "Int\n";
			break;
		default:
			std::cout << "Error\n";
			exit(1);
	}
}

enum struct Mode_cl
{
	Ana,
	Syn,
	Plus
};

struct Mode
{
	Mode_cl cl;
	Typ ty;

	Mode(Mode_cl cl, Typ ty) : cl(cl), ty(ty) {}
	Mode &operator=(Mode &m)
	{
		cl = m.cl;
		ty = m.ty;
		return *this;
	}
};

class Thr
{
	//context for the future
	private:
		Exp *exp;
		Mode mode;
		bool done;
		Typ res;
		std::vector<Thr*> dep;
	public:
		Thr(Exp *exp, Mode mode) : exp(exp), mode(mode), done(false), dep(std::vector<Thr*>()) {}
		Thr(std::vector<Thr*> &dep, Mode mode) : exp(nullptr), dep(dep), mode(mode), done(false) {}
		Exp *getExp();
		Mode getMode();
		void setMode(Mode m);
		void setDone(bool q);
		bool getDone();
		void setRes(Typ ty);
		Typ getRes();
		std::vector<Thr*> &getDep();
		void setDep(std::vector<Thr*> &v);
		bool depEmpty();
		void set(Thr &th);
		~Thr()
		{
			std::cout << "deleting thr\n"; 
		}
};

Exp* Thr::getExp()
{
	return exp;
}

Mode Thr::getMode()
{
	return mode;
}

void Thr::setMode(Mode m)
{
	mode = m;
}

void Thr::setDone(bool q)
{
	done = q;
}

bool Thr::getDone()
{
	return done;
}

void Thr::setRes(Typ ty)
{
	res = ty;
}

Typ Thr::getRes()
{
	return res;
}

std::vector<Thr*> &Thr::getDep()
{
	return dep;
}

void Thr::setDep(std::vector<Thr*> &d)
{
	dep = d;
}

bool Thr::depEmpty()
{
	return dep.empty();
}

class Proc
{
	private:
		int id;
		std::vector<std::deque<Thr*>> *pool;
		Thr *current;

	public:
		Proc(int id, std::vector<std::deque<Thr*>> *pool) : id(id), pool(pool), current(nullptr) {}

		void set_current(Thr *thr);
		void dq_push_back(Thr *thr);
		Thr *dq_pop_back();
		void process_thr();
		Thr *spawn_thr(Exp *ex, Mode m);
		//Thr *join_thr(std::vector<Thr*> v, Mode m);
		void end_thr(Typ res);
		void stall_thr();
		
		void synth(Exp *exp);
		void synth_plus(Typ ty1, Typ ty2);
};

void Proc::set_current(Thr *thr)
{
	current = thr;
}

void Proc::dq_push_back(Thr *thr)
{
	(*pool)[id].push_back(thr);
}

Thr *Proc::dq_pop_back()
{
	Thr *t = (*pool)[id].back();
	(*pool)[id].pop_back();
	return t;
}

//static int co = 0;

void Proc::process_thr()
{
	std::cout << "process thr\n";
	current->getExp()->printCl();
	Mode m = current->getMode();
	/*if(co < 3)
	{
		++co;
	}
	else
	{
		exit(1);
	}*/

	switch(m.cl)
	{
		case Mode_cl::Syn:
			std::cout << "MODE SYN\n";
			synth(current->getExp());
			break;
		case Mode_cl::Plus:
		{
			std::cout << "MODE PLUS\n";
			std::vector<Thr*> dep = current->getDep();
			if(dep[0]->getDone() && dep[1]->getDone())
			{
				synth_plus(dep[0]->getRes(), dep[1]->getRes());
				delete dep[0];
				delete dep[1];
			}
			else
			{
				stall_thr();
			}
			break;
		}
		default:
			std::cout << "Error\n";
			//exit(1);
			break;
	}
}

Thr *Proc::spawn_thr(Exp *ex, Mode m)
{
	return new Thr(ex, m);
}

/*Thr *Proc::join_thr(std::vector<Thr*> &v, Mode m)
{
	return new Thr(v, m);
}*/

void Proc::end_thr(Typ ty)
{
	std::cout << "End\n";
	current->getExp()->printCl();
	current->setRes(ty);
	current->setDone(true);
	if((*pool)[id].empty())
	{
		//work steal
	}
	else
	{
		current = dq_pop_back();
		std::cout << "New current\n";
		current->getExp()->printCl();
		process_thr();
	}
}

void Proc::stall_thr()
{	
	if((*pool)[id].empty())
	{
		//work steal
	}
	else
	{
		Thr *thr = dq_pop_back();
		dq_push_back(current);
		current = thr;
	}
}

void Proc::synth(Exp *exp)
{
	std::cout << "SYNTH\n";
	switch(exp->getCl())
	{
		case Exp_cl::Bool:
		        end_thr(Typ::Bool);
			break;
		case Exp_cl::Int:
			std::cout << "SYNTH END INT\n";
			end_thr(Typ::Int);
			break;
		case Exp_cl::Plus:
		{
			std::cout << "SYNTH PLUS\n";
			Plus *ex = dynamic_cast<Plus*>(exp);
			Thr *thr1 = spawn_thr(ex->getE1(), Mode(Mode_cl::Syn, Typ::None));
			Thr *thr2 = spawn_thr(ex->getE2(), Mode(Mode_cl::Syn, Typ::None));
			std::vector<Thr*> v = {thr1, thr2};
			current->setDep(v);
			current->setMode(Mode(Mode_cl::Plus, Typ::None));
			Thr *thr3 = current;
			current = thr1;
			dq_push_back(thr3);
			dq_push_back(thr2);
			process_thr();
			break;
		}
		default:
			std::cout << "Invalid expression\n";
			//exit(1);
			break;
	}
}

void Proc::synth_plus(Typ ty1, Typ ty2)
{
	if(ty1 == Typ::Int && ty2 == Typ::Int)
	{
		end_thr(Typ::Int);
	}
	else
	{
		end_thr(Typ::None);
	}	
}

int main(int argc, char **argv)
{
	int num = omp_get_max_threads();
	std::vector<std::deque<Thr*>> pool(num);
	std::cout << num << "\n";
	
	Proc p = Proc(0, &pool);
	Exp *e1 = new Exp(Exp_cl::Int);
	Exp *e2 = new Exp(Exp_cl::Int);
	Exp *pl = new Plus(e1, e2);
	Thr* thr = new Thr(pl, Mode(Mode_cl::Syn, Typ::None));
	p.set_current(thr);
	p.process_thr();
	
	std::cout << "Yabo\n";
	printTyp(thr->getRes());
	delete thr;
	delete pl;
	/*#pragma omp parallel shared(pool)
	{
		int id = omp_get_thread_num();
		std::cout << id << "\n";

		if(id == 0)
		{

		}
	}*/
	return 0;
}
