# Fine-Tuning Workflow

This folder prepares a LoRA fine-tune for the natural-language `@` command
translator.

The current laptop is not a good training target for Qwen-size models because
it has Python 3.7, no ML fine-tuning packages, and no visible GPU. Run these
steps on a GPU machine such as Colab, Kaggle, or a Linux box with CUDA.

## 1. Generate Data

From `busybox_shell`:

```sh
make
./generate_training_data.sh
cd training
python3 prepare_finetune_data.py
```

## 2. Install Trainer Dependencies

Use Python 3.10 or newer:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
```

## 3. Train LoRA Adapter

```sh
python finetune_lora.py \
  --model Qwen/Qwen2.5-3B-Instruct \
  --data mysh_train_chat.jsonl \
  --output mysh-qwen2.5-3b-lora
```

## 4. Import Into Ollama

Copy `Modelfile.adapter.example` to `Modelfile`, then create a local Ollama
model:

```sh
cp Modelfile.adapter.example Modelfile
ollama create mysh-trained -f Modelfile
ollama run mysh-trained
```

Then run the shell with:

```sh
OLLAMA_MODEL="mysh-trained" ./busybox_shell
```

Keep the live helper context even after training. Fine-tuning teaches the
model your preferred style; live help/JSON keeps it accurate as commands
change.
