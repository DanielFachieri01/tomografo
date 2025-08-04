#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TCanvas.h>
#include <TString.h>
#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <sstream>
#include <TF1.h>
#include <TLatex.h>

void input_gain() {
    std::vector<std::tuple<double, double, double, std::string>> conditions = {
        {1.0, 1, 70, "Ar"},
        {3.0, 1, 70, "Ar"},
        {3.0, 2, 70, "Ar"},
        {1.0, 1, 90, "Kr"},
        {3.0, 1, 90, "Kr"}
    };

    std::vector<std::string> names;
    for (const auto& [window, pressure, proportion, type] : conditions) {
        std::ostringstream oss;
        oss << type << "_" << proportion << "_" << (100 - proportion)
            << "_" << pressure << "atm_" << window << "cm";
        names.push_back(oss.str());
    }

    std::vector<std::string> multi_names = { "Ar_70_30_1atm_1cm", "Ar_70_30_1atm_1cm", "Ar_70_30_2atm_3cm", "Kr_90_10_1atm_1cm", "Kr_90_10_1atm_1cm" };
    
    std::vector<std::tuple<int,int>> limits = {
        {2500, 3500}, {2200, 3700}, {1500, 2000}, {500, 1000}, {500, 1000}
    }; 

    

    TFile* file = TFile::Open("no_gain.root");
    TFile* file2 = TFile::Open("ganho.root");

    for (size_t nome_idx = 0; nome_idx < names.size(); ++nome_idx) {
        std::cout << "Processando: " << names[nome_idx] << std::endl;
        
        int xmin = std::get<0>(limits[nome_idx]);
        int xmax = std::get<1>(limits[nome_idx]);
        TF1* gauss = new TF1("gauss", "gaus", xmin, xmax);


        // Abrir a TTree de elétrons primários
        TTree* tree = (TTree*) file->Get(names[nome_idx].c_str());
        if (!tree) {
            std::cerr << "Erro: não encontrou TTree '" << names[nome_idx] << "' em no_gain.root\n";
            continue;
        }

        // Abrir a TTree de ganhos
        TTree* gain_tree = (TTree*) file2->Get(multi_names[nome_idx].c_str());
        if (!gain_tree) {
            std::cerr << "Erro: não encontrou TTree '" << multi_names[nome_idx] << "' em ganho.root\n";
            continue;
        }

        int nelectrons = 0;
        int ganho = 0;

        tree->SetBranchAddress("gain", &nelectrons);
        gain_tree->SetBranchAddress("gain", &ganho);

        TH1F* pdf = new TH1F(("pdf_" + multi_names[nome_idx]).c_str(), "Gain PDF", 300, 0, 200);
        Long64_t nentries_gain = gain_tree->GetEntries();
        for (Long64_t j = 0; j < nentries_gain; ++j) {
            gain_tree->GetEntry(j);
            pdf->Fill(ganho);
        }
        pdf->Scale(1.0 / pdf->Integral(), "width");

        TH1F* spectra = new TH1F(("spectra_" + names[nome_idx]).c_str(), names[nome_idx].c_str(), 512, 0,4096);
        TH1F* primary = new TH1F(("primary_" + names[nome_idx]).c_str(), "Primary Electrons", 512, 0, 1024);

        Long64_t nentries = tree->GetEntries();
        for (Long64_t j = 0; j < nentries; ++j) {
            tree->GetEntry(j);

            int sum = 0;
            for (int k = 0; k < nelectrons; ++k) {
                sum += pdf->GetRandom();
            }
            spectra->Fill(sum);
        }

        TCanvas* c_pdf = new TCanvas(("c_pdf_" + multi_names[nome_idx]).c_str(), "PDF Canvas", 800, 600);
        pdf->GetYaxis()->SetTitle("Probability");
        pdf->GetXaxis()->SetTitle("Multiplication Factor");
        pdf->Draw();
        c_pdf->SetLogy();
        c_pdf->SaveAs(("../results/pdfs/ganho_" + multi_names[nome_idx] + ".pdf").c_str());
        c_pdf->SaveAs(("../results/pngs/ganho_" + multi_names[nome_idx] + ".png").c_str());
        delete c_pdf;

        TCanvas* c = new TCanvas(("c_" + names[nome_idx]).c_str(), "Canvas", 800, 600);
        spectra->GetYaxis()->SetTitle("Counts");
        spectra->GetXaxis()->SetTitle("Spectrum [# electrons]");
        spectra->Fit(gauss, "R");
        spectra->Draw();
        gauss->SetLineColor(kRed);
        gauss->SetLineWidth(2);
        gauss->Draw("same");
        double amplitude = gauss->GetParameter(0);  // altura do pico
        double mean     = gauss->GetParameter(1);  // média
        double sigma    = gauss->GetParameter(2);  // sigma
        // Escreva o valor de sigma perto do pico
        TLatex latex;
        latex.SetNDC(false);         // Coordenadas no espaço do eixo (x, y)
        latex.SetTextSize(0.03);
        latex.DrawLatex(mean + sigma, amplitude *1.2, Form("FWHM/<E> = %.1f% %", (2.355*sigma/mean)*100));
   

        c->SaveAs(("../results/pdfs/" + names[nome_idx] + ".pdf").c_str());
        c->SaveAs(("../results/pngs/" + names[nome_idx] + ".png").c_str());

        delete pdf;
        delete spectra;
        delete c;
    }

    file->Close();
    file2->Close();
}
